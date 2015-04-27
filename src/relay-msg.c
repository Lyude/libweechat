/* ©2015 Stephen Chandler Paul <thatslyude@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.

 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 */
#include "libweechat.h"
#include "relay-msg.h"
#include "misc.h"

#include <glib.h>
#include <gio/gio.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

GHashTable *cmd_identifiers;
GHashTable *type_identifiers;

struct _LibWCRelayMessageData {
	gint ref_count;
	void *data;
	void *data_endptr;
};

typedef struct _LibWCRelayMessageData LibWCRelayMessageData;

typedef GVariant* (*LibWCObjectExtractor)(LibWCRelayMessageData*,
										  void**,
										  GError**);

#define GET_FIELD(data_, offset_, type_) (*((type_*)(&((gint8*)data_)[offset_])))

#define OBJECT_ID_LEN  ((gsize)3)
#define OBJECT_INT_LEN ((gsize)4)

/* LEN_LEN = literally the length of the length. The length of the length field
 * for an object in the weechat protocol varies depending on the actual object
 * in question.
 */
#define OBJECT_LONG_LEN_LEN    ((gsize)1)
#define OBJECT_STRING_LEN_LEN  ((gsize)4)
#define OBJECT_BUFFER_LEN_LEN  ((gsize)4)
#define OBJECT_POINTER_LEN_LEN ((gsize)1)
#define OBJECT_TIME_LEN_LEN    ((gsize)1)
#define OBJECT_ARRAY_LEN_LEN   ((gsize)4)

/* Variant types for objects that can't use primitive variants */
#define OBJECT_STRING_VARIANT_TYPE (G_VARIANT_TYPE("ms"))
#define OBJECT_BUFFER_VARIANT_TYPE (G_VARIANT_TYPE("may"))

static LibWCRelayMessageData *
libwc_relay_message_data_copy(LibWCRelayMessageData *data) {
	data->ref_count++;

	return data;
}

static void
libwc_relay_message_data_unref(LibWCRelayMessageData *data) {
	if (--data->ref_count != 0)
		return;

	g_free(data->data);
	g_free(data);
}

G_DEFINE_BOXED_TYPE(LibWCRelayMessageData, libwc_relay_message_data,
					libwc_relay_message_data_copy, libwc_relay_message_data_unref);

static inline gboolean
check_msg_bounds(const LibWCRelayMessageData *data,
				 const void *pos,
				 goffset offset,
				 GError **error) {
	if (G_LIKELY(pos + offset <= data->data_endptr &&
				 offset > 0))
		return TRUE;

	g_set_error_literal(error, LIBWC_ERROR_RELAY,
						LIBWC_ERROR_RELAY_UNEXPECTED_EOM,
						"Message received from relay was shorter then expected");
	return FALSE;
}

static LibWCObjectExtractor
get_extractor_for_object_type(LibWCRelayObjectType type);

static LibWCRelayObjectType
extract_object_type(const LibWCRelayMessageData *data,
					void **pos,
					GError **error) {
	LibWCRelayObjectType type;
	gchar type_str[4] = {0};

	g_return_val_if_fail(check_msg_bounds(data, *pos, OBJECT_ID_LEN, error), 0);

	memcpy(type_str, *pos, OBJECT_ID_LEN);
	*pos += OBJECT_ID_LEN;

	type = (LibWCRelayObjectType)g_hash_table_lookup(type_identifiers, type_str);
	if (!type) {
		gchar *data_type = g_strescape(type_str, NULL);

		g_warn_if_reached();
		g_set_error(error, LIBWC_ERROR_RELAY, LIBWC_ERROR_RELAY_INVALID_DATA,
					"Unknown data type encountered: '%s'", data_type);

		g_free(data_type);

		return 0;
	}

	return type;
}

static gboolean
extract_size(const LibWCRelayMessageData *data,
			 void **pos,
			 gsize size_len,
			 gint32 *size,
			 GError **error) {
	g_return_val_if_fail(check_msg_bounds(data, *pos, size_len, error), FALSE);

	/* We're copying a big endian value, so we need to start from the end of the
	 * integer, not the start
	 */
	memcpy((gchar*)size + (sizeof(gint32) - size_len), *pos, size_len);
	*pos += size_len;

	*size = GINT32_FROM_BE(*size);

	return TRUE;
}

static inline gboolean
object_type_is_primitive(LibWCRelayObjectType type) {
	gboolean is_primitive;

	switch (type) {
		case LIBWC_OBJECT_TYPE_CHAR:
		case LIBWC_OBJECT_TYPE_INT:
		case LIBWC_OBJECT_TYPE_LONG:
		case LIBWC_OBJECT_TYPE_STRING:
		case LIBWC_OBJECT_TYPE_BUFFER:
		case LIBWC_OBJECT_TYPE_POINTER:
		case LIBWC_OBJECT_TYPE_TIME:
			is_primitive = TRUE;
			break;
		default:
			is_primitive = FALSE;
			break;
	}

	return is_primitive;
} G_GNUC_PURE

static const GVariantType*
get_variant_type_for_primitive_object_type(LibWCRelayObjectType type) {
	const GVariantType *variant_type;

	switch (type) {
		case LIBWC_OBJECT_TYPE_CHAR:
			variant_type = G_VARIANT_TYPE_BYTE;
			break;
		case LIBWC_OBJECT_TYPE_INT:
			variant_type = G_VARIANT_TYPE_INT32;
			break;
		case LIBWC_OBJECT_TYPE_LONG:
			variant_type = G_VARIANT_TYPE_INT64;
			break;
		case LIBWC_OBJECT_TYPE_STRING:
			variant_type = OBJECT_STRING_VARIANT_TYPE;
			break;
		case LIBWC_OBJECT_TYPE_BUFFER:
			variant_type = OBJECT_BUFFER_VARIANT_TYPE;
			break;
		case LIBWC_OBJECT_TYPE_POINTER:
		case LIBWC_OBJECT_TYPE_TIME:
			variant_type = G_VARIANT_TYPE_UINT64;
			break;
		default:
			variant_type = NULL;
			break;
	}

	g_assert_nonnull(variant_type);

	return variant_type;
}

/* Used to read a string of data from a message. A string in a weechat message
 * might not necessarily be character data, and may be something else, hence the
 * convience function here. This function increments pos on it's own
 */
static void *
extract_string(LibWCRelayMessageData *data,
			   void **pos,
			   gsize strlen_field_len,
			   GError **error) {
	gint32 len = 0;
	void *str;

	if (!extract_size(data, pos, strlen_field_len, &len, error))
		return FALSE;

	g_return_val_if_fail(check_msg_bounds(data, *pos, len, error), NULL);

	str = g_malloc0(len + 1);
	memcpy(str, *pos, len);
	*pos += len;

	return str;
}

static GVariant *
extract_normal_object(LibWCRelayMessageData *data,
					  const GVariantType *value_type,
					  gsize size,
					  void **pos,
					  GError **error) {
	GVariant *object;

	g_return_val_if_fail(check_msg_bounds(data, *pos, size, error), NULL);

	object = g_variant_new_from_data(value_type, *pos, size, FALSE,
									 (GDestroyNotify)libwc_relay_message_data_unref,
									 libwc_relay_message_data_copy(data));
	*pos += size;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
	/* On little endian systems we need to do a byteswap */
	if (size > 1)
		object = g_variant_byteswap(object);
#endif

	return object;
}

static GVariant *
extract_char_object(LibWCRelayMessageData *data,
					void **pos,
					GError **error) {
	return extract_normal_object(data, G_VARIANT_TYPE_BYTE, sizeof(gchar), pos,
								 error);
}

static GVariant *
extract_int_object(LibWCRelayMessageData *data,
				   void **pos,
				   GError **error) {
	return extract_normal_object(data, G_VARIANT_TYPE_INT32, sizeof(gint32),
								 pos, error);
}

static GVariant *
extract_long_object(LibWCRelayMessageData *data,
					void **pos,
					GError **error) {
	GVariant *object;
	gchar *str;
	glong value;

	str = extract_string(data, pos, OBJECT_LONG_LEN_LEN, error);
	if (!str)
		return NULL;

	value = strtol(str, NULL, 10);
	if (value == 0 && (errno == EINVAL || errno == ERANGE)) {
		g_warn_if_reached();
		g_set_error(error, LIBWC_ERROR_RELAY, LIBWC_ERROR_RELAY_INVALID_DATA,
					"Failed to parse object of type 'long': %s",
					strerror(errno));
		return NULL;
	}

	g_free(str);

	object = g_variant_new_int64(value);

	return object;
}

static GVariant *
extract_string_object(LibWCRelayMessageData *data,
					  void **pos,
					  GError **error) {
	GVariant *object, *maybe_container;
	gchar *str;
	gint32 len = 0;

	if (!extract_size(data, pos, OBJECT_STRING_LEN_LEN, &len, error))
		return NULL;

	if (len == 0)
		object = g_variant_new_string("");
	else if (len == -1)
		object = NULL;
	else {
		g_return_val_if_fail(check_msg_bounds(data, *pos, len, error), NULL);

		str = g_new0(gchar, len + 1);
		memcpy(str, *pos, len);

		object = g_variant_new_take_string(str);
		*pos += len;
	}

	maybe_container = g_variant_new_maybe(G_VARIANT_TYPE_STRING, object);

	return maybe_container;
}

static GVariant *
extract_buffer_object(LibWCRelayMessageData *data,
					  void **pos,
					  GError **error) {
	GVariant *object, *maybe_container;
	gint32 len = 0;

	if (!extract_size(data, pos, OBJECT_BUFFER_LEN_LEN, &len, error))
		return NULL;

	if (len == 0)
		object = g_variant_new_bytestring("");
	else if (len == -1)
		object = NULL;
	else {
		g_return_val_if_fail(check_msg_bounds(data, *pos, len, error), NULL);

		object =
			g_variant_new_from_data(G_VARIANT_TYPE_BYTESTRING, *pos, len, FALSE,
									(GDestroyNotify)libwc_relay_message_data_unref,
									libwc_relay_message_data_copy(data));
		*pos += len;
	}

	maybe_container = g_variant_new_maybe(G_VARIANT_TYPE_BYTESTRING, object);

	return maybe_container;
}

static GVariant *
extract_pointer_object(LibWCRelayMessageData *data,
					   void **pos,
					   GError **error) {
	GVariant *object;
	guint64 value;
	gint32 len = 0;

	if (!extract_size(data, pos, OBJECT_POINTER_LEN_LEN, &len, error))
		return NULL;

	g_return_val_if_fail(check_msg_bounds(data, *pos, len, error), NULL);

	/* If we have a length of 01, and the next byte is 0, we have a NULL
	 * pointer */
	if (len == 1 && GET_FIELD(*pos, 1, guint8) == 0) {
		object = g_variant_new_uint64(0);
		*pos += 1;
	}
	else {
		gchar str[len+1];

		memcpy(str, *pos, len);

		/* Null terminate the end of the string */
		str[len] = 0;

		value = strtoul(str, NULL, 16);
		if (value == 0 && (errno == EINVAL || errno == ERANGE)) {
			g_warn_if_reached();
			g_set_error(error, LIBWC_ERROR_RELAY, LIBWC_ERROR_RELAY_INVALID_DATA,
						"Failed to parse object of type 'pointer': %s",
						strerror(errno));
			return NULL;
		}

		object = g_variant_new_uint64(value);
		*pos += len;
	}

	return object;
}

static GVariant *
extract_time_object(LibWCRelayMessageData *data,
					void **pos,
					GError **error) {
	GVariant *object;
	gchar *str;
	guint64 value;
	gsize len = 0;

	str = extract_string(data, pos, OBJECT_TIME_LEN_LEN, error);
	if (!str)
		return NULL;

	value = strtoul(str, NULL, 10);
	if (value == 0 && (errno == EINVAL || errno == ERANGE)) {
		g_warn_if_reached();
		g_set_error(error, LIBWC_ERROR_RELAY, LIBWC_ERROR_RELAY_INVALID_DATA,
					"Failed to parse object of type 'time': %s",
					strerror(errno));
		return NULL;
	}

	g_free(str);

	object = g_variant_new_uint64(value);

	return object;
}

static GVariant *
extract_array_object(LibWCRelayMessageData *data,
					 void **pos,
					 GError **error) {
	GVariant *variant,
	         **array_contents, **tuple_contents;
	LibWCRelayObjectType object_type;
	const GVariantType* variant_type;
	LibWCObjectExtractor element_extractor;
	gint32 count = 0;

	object_type = extract_object_type(data, pos, error);
	if (!object_type)
		return NULL;
	g_return_val_if_fail(object_type_is_primitive(object_type), NULL);

	if (!extract_size(data, pos, OBJECT_ARRAY_LEN_LEN, &count, error))
		return NULL;

	element_extractor = get_extractor_for_object_type(object_type);
	array_contents = g_new0(GVariant*, count);

	for (int i = 0; i < count; i++) {
		array_contents[i] = element_extractor(data, pos, error);
		if (!array_contents[i]) {
			for (int i = 0; i < count || array_contents[i] == NULL; i++)
				g_variant_unref(array_contents[i]);

			return NULL;
		}
	}

	variant_type = get_variant_type_for_primitive_object_type(object_type);

	tuple_contents = (GVariant*[]) {
		g_variant_new_byte(object_type),
		g_variant_new_array(variant_type, array_contents, count)
	};
	variant = g_variant_new_tuple(tuple_contents, 2);

	g_free(array_contents);

	return variant;
}

static LibWCObjectExtractor
get_extractor_for_object_type(LibWCRelayObjectType type) {
	LibWCObjectExtractor extractor;

	switch(type) {
		case LIBWC_OBJECT_TYPE_CHAR:
			extractor = extract_char_object;
			break;
		case LIBWC_OBJECT_TYPE_INT:
			extractor = extract_int_object;
			break;
		case LIBWC_OBJECT_TYPE_LONG:
			extractor = extract_long_object;
			break;
		case LIBWC_OBJECT_TYPE_STRING:
			extractor = extract_string_object;
			break;
		case LIBWC_OBJECT_TYPE_BUFFER:
			extractor = extract_buffer_object;
			break;
		case LIBWC_OBJECT_TYPE_POINTER:
			extractor = extract_pointer_object;
			break;
		case LIBWC_OBJECT_TYPE_TIME:
			extractor = extract_time_object;
			break;
		case LIBWC_OBJECT_TYPE_ARRAY:
			extractor = extract_array_object;
			break;
		/* TODO: Support the rest of the object types */
		default:
			extractor = NULL;
			break;
	}

	return extractor;
} G_GNUC_PURE

static LibWCRelayMessageObject *
extract_object(LibWCRelayMessageData *data,
			   void **pos,
			   GError **error) {
	GVariant *variant;
	LibWCRelayMessageObject *object;
	LibWCObjectExtractor extractor;
	LibWCRelayObjectType type;

	type = extract_object_type(data, pos, error);
	if (!type)
		return NULL;

	extractor = get_extractor_for_object_type(type);
	variant = extractor(data, pos, error);
	if (!variant)
		return NULL;

	object = g_slice_alloc(sizeof(object));
	object->type = type;
	object->value = variant;

	return object;
}

static GList *
extract_objects(LibWCRelayMessageData *data,
				GError **error) {
	LibWCRelayMessageObject *object;
	GList *objects = NULL;
	void *pos = data->data;

	while (pos < data->data_endptr) {
		object = extract_object(data, &pos, error);
		if (!object)
			goto extract_objects_error;

		objects = g_list_prepend(objects, object);
	}

	objects = g_list_reverse(objects);

	return objects;

extract_objects_error:
	g_warn_if_reached();

	/* TODO: free memory here */
	return NULL;
}

LibWCRelayMessage *
libwc_relay_message_parse_data(void *data,
							   gsize size,
							   GError **error) {
	LibWCRelayMessage *message = g_new(LibWCRelayMessage, 1);
	LibWCRelayMessageData *message_data = g_new(LibWCRelayMessageData, 1);

	message_data->ref_count = 1;
	message_data->data = data;
	message_data->data_endptr = data + size;

	message->id = 0;
	message->objects = extract_objects(message_data, error);

	if (!message->objects)
		goto libwc_relay_message_parse_data_error;

	libwc_relay_message_data_unref(message_data);

	return message;

libwc_relay_message_parse_data_error:
	/* TODO: Fix memory leaks here */
	g_free(message);
	libwc_relay_message_data_unref(message_data);

	return NULL;
}

static inline void
init_object_type(gchar *str,
				 LibWCRelayObjectType type) {
	g_hash_table_insert(type_identifiers, str, GINT_TO_POINTER(type));
}

void
_libwc_init_msg_parser() LIBWC_CONSTRUCTOR;

void
_libwc_init_msg_parser() {
	cmd_identifiers = g_hash_table_new(g_str_hash, g_str_equal);
	type_identifiers = g_hash_table_new(g_str_hash, g_str_equal);

	init_object_type("chr", LIBWC_OBJECT_TYPE_CHAR);
	init_object_type("int", LIBWC_OBJECT_TYPE_INT);
	init_object_type("lon", LIBWC_OBJECT_TYPE_LONG);
	init_object_type("str", LIBWC_OBJECT_TYPE_STRING);
	init_object_type("buf", LIBWC_OBJECT_TYPE_BUFFER);
	init_object_type("ptr", LIBWC_OBJECT_TYPE_POINTER);
	init_object_type("tim", LIBWC_OBJECT_TYPE_TIME);
	init_object_type("htb", LIBWC_OBJECT_TYPE_HASHTABLE);
	init_object_type("hda", LIBWC_OBJECT_TYPE_HDATA);
	init_object_type("inf", LIBWC_OBJECT_TYPE_INFO);
	init_object_type("inl", LIBWC_OBJECT_TYPE_INFOLIST);
	init_object_type("arr", LIBWC_OBJECT_TYPE_ARRAY);
}
