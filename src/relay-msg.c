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

GHashTable *event_identifiers;
GHashTable *type_identifiers;

typedef GVariant* (*LibWCObjectExtractor)(void**,
                                          const void*,
                                          GError**);

#define GET_FIELD(data_, offset_, type_) (*((type_*)(&((gint8*)data_)[offset_])))

#define OBJECT_ID_LEN  ((gsize)3)
#define OBJECT_INT_LEN ((gsize)4)

/* LEN_LEN = literally the length of the length. The length of the length field
 * for an object in the weechat protocol varies depending on the actual object
 * in question.
 */
#define OBJECT_LONG_LEN_LEN             ((gsize)1)
#define OBJECT_STRING_LEN_LEN           ((gsize)4)
#define OBJECT_BUFFER_LEN_LEN           ((gsize)4)
#define OBJECT_POINTER_LEN_LEN          ((gsize)1)
#define OBJECT_TIME_LEN_LEN             ((gsize)1)
#define OBJECT_ARRAY_LEN_LEN            ((gsize)4)
#define OBJECT_HASHTABLE_LEN_LEN        ((gsize)4)
#define OBJECT_HDATA_LEN_LEN            ((gsize)4)
#define OBJECT_HDATA_HPATH_LEN_LEN      ((gsize)4)
#define OBJECT_HDATA_KEY_STRING_LEN_LEN ((gsize)4)
#define OBJECT_INFOLIST_LEN_LEN         ((gsize)4)

/* Variant types for objects that can't use primitive variants */
#define OBJECT_STRING_VARIANT_TYPE (G_VARIANT_TYPE("ms"))
#define OBJECT_BUFFER_VARIANT_TYPE (G_VARIANT_TYPE("may"))

static inline gboolean
check_msg_bounds(const void *pos,
                 const void *end_ptr,
                 goffset offset,
                 GError **error) {
    if (G_LIKELY(pos + offset <= end_ptr &&
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
get_object_type_from_string(const gchar *str,
                            GError **error) {
    LibWCRelayObjectType type;

    type = (LibWCRelayObjectType)g_hash_table_lookup(type_identifiers, str);
    if (!type) {
        gchar *data_type = g_strescape(str, NULL);

        g_warn_if_reached();
        g_set_error(error, LIBWC_ERROR_RELAY, LIBWC_ERROR_RELAY_INVALID_DATA,
                    "Unknown data type encountered: '%s'", data_type);

        g_free(data_type);

        return 0;
    }

    return type;
}

static LibWCRelayObjectType
extract_object_type(void **pos,
                    const void *end_ptr,
                    GError **error) {
    LibWCRelayObjectType type;
    gchar type_str[4] = {0};

    g_return_val_if_fail(check_msg_bounds(*pos, end_ptr, OBJECT_ID_LEN, error), 0);

    memcpy(type_str, *pos, OBJECT_ID_LEN);
    *pos += OBJECT_ID_LEN;

    type = get_object_type_from_string(type_str, error);
    if (!type)
        return 0;

    return type;
}

static gboolean
extract_size(void **pos,
             const void *end_ptr,
             gsize size_len,
             gint32 *size,
             GError **error) {
    g_return_val_if_fail(check_msg_bounds(*pos, end_ptr, size_len, error), FALSE);

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

static gchar *
extract_string(void **pos,
               const void *end_ptr,
               gsize strlen_field_len,
               gboolean allow_null,
               GError **error) {
    gint32 len = 0;
    void *str;

    g_assert_null(*error);

    if (!extract_size(pos, end_ptr, strlen_field_len, &len, error))
        return FALSE;

    if (allow_null) {
        if (len == -1)
            return NULL;

        if (len != 0) {
            g_return_val_if_fail(check_msg_bounds(*pos, end_ptr, len, error),
                                 NULL);
        }
    }
    else
        g_return_val_if_fail(check_msg_bounds(*pos, end_ptr, len, error), NULL);

    str = g_malloc0(len + 1);
    memcpy(str, *pos, len);
    *pos += len;

    return str;
}

static GVariant *
extract_normal_object(void **pos,
                      const void *end_ptr,
                      const GVariantType *value_type,
                      gsize size,
                      GDestroyNotify notify,
                      GError **error) {
    GVariant *object;
    void *data;

    g_return_val_if_fail(check_msg_bounds(*pos, end_ptr, size, error), NULL);

    data = g_slice_copy(size, *pos);
    object = g_variant_new_from_data(value_type, data, size, FALSE, notify,
                                     data);
    *pos += size;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    /* On little endian systems we need to do a byteswap */
    if (size > 1)
        object = g_variant_byteswap(object);
#endif

    return object;
}

static void
free_char_object(void *mem) { g_slice_free(gchar, mem); }

static GVariant *
extract_char_object(void **pos,
                    const void *end_ptr,
                    GError **error) {
    return extract_normal_object(pos, end_ptr, G_VARIANT_TYPE_BYTE,
                                 sizeof(gchar), free_char_object, error);
}

static void
free_int_object(void *mem) { g_slice_free(gint32, mem); }

static GVariant *
extract_int_object(void **pos,
                   const void *end_ptr,
                   GError **error) {
    return extract_normal_object(pos, end_ptr, G_VARIANT_TYPE_INT32,
                                 sizeof(gint32), free_int_object, error);
}

static GVariant *
extract_long_object(void **pos,
                    const void *end_ptr,
                    GError **error) {
    GVariant *object;
    gchar *str;
    glong value;

    str = extract_string(pos, end_ptr, OBJECT_LONG_LEN_LEN, FALSE, error);
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
extract_string_object(void **pos,
                      const void *end_ptr,
                      GError **error) {
    GVariant *object = NULL, *maybe_container;
    gchar *str = NULL;

    str = extract_string(pos, end_ptr, OBJECT_STRING_LEN_LEN, TRUE, error);
    if (*error)
        return NULL;

    if (str)
        object = g_variant_new_take_string(str);

    maybe_container = g_variant_new_maybe(G_VARIANT_TYPE_STRING, object);

    return maybe_container;
}

static GVariant *
extract_buffer_object(void **pos,
                      const void *end_ptr,
                      GError **error) {
    GVariant *object, *maybe_container;
    void *data;
    gint32 len = 0;

    if (!extract_size(pos, end_ptr, OBJECT_BUFFER_LEN_LEN, &len, error))
        return NULL;

    if (len == 0)
        object = g_variant_new_bytestring("");
    else if (len == -1)
        object = NULL;
    else {
        g_return_val_if_fail(check_msg_bounds(*pos, end_ptr, len, error), NULL);

        data = g_memdup(*pos, len);
        object =
            g_variant_new_from_data(OBJECT_BUFFER_VARIANT_TYPE, data, len,
                                    FALSE, g_free, data);
        *pos += len;
    }

    maybe_container = g_variant_new_maybe(G_VARIANT_TYPE_BYTESTRING, object);

    return maybe_container;
}

static GVariant *
extract_pointer_object(void **pos,
                       const void *end_ptr,
                       GError **error) {
    GVariant *object;
    guint64 value;
    gint32 len = 0;

    if (!extract_size(pos, end_ptr, OBJECT_POINTER_LEN_LEN, &len, error))
        return NULL;

    g_return_val_if_fail(check_msg_bounds(*pos, end_ptr, len, error), NULL);

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
extract_time_object(void **pos,
                    const void *end_ptr,
                    GError **error) {
    GVariant *object;
    gchar *str;
    guint64 value;
    gsize len = 0;

    str = extract_string(pos, end_ptr, OBJECT_TIME_LEN_LEN, FALSE, error);
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
extract_array_object(void **pos,
                     const void *end_ptr,
                     GError **error) {
    GVariant *variant,
             **array_contents, **tuple_contents;
    LibWCRelayObjectType object_type;
    const GVariantType* variant_type;
    LibWCObjectExtractor element_extractor;
    gint32 count = 0;

    object_type = extract_object_type(pos, end_ptr, error);
    if (!object_type)
        return NULL;
    g_return_val_if_fail(object_type_is_primitive(object_type), NULL);

    if (!extract_size(pos, end_ptr, OBJECT_ARRAY_LEN_LEN, &count, error))
        return NULL;

    element_extractor = get_extractor_for_object_type(object_type);
    array_contents = g_new0(GVariant*, count);

    for (int i = 0; i < count; i++) {
        array_contents[i] = element_extractor(pos, end_ptr, error);
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

static GVariant *
extract_hashtable_object(void **pos,
                         const void *end_ptr,
                         GError **error)
{
    GVariant *variant,
             *key, *value;
    GVariant **entries = NULL;
    LibWCRelayObjectType key_type, value_type;
    LibWCObjectExtractor key_extractor, value_extractor;
    gint32 count;

    key_type = extract_object_type(pos, end_ptr, error);
    if (!key_type)
        return NULL;
    g_return_val_if_fail(object_type_is_primitive(key_type), NULL);

    value_type = extract_object_type(pos, end_ptr, error);
    if (!value_type)
        return NULL;
    g_return_val_if_fail(object_type_is_primitive(value_type), NULL);

    if (!extract_size(pos, end_ptr, OBJECT_HASHTABLE_LEN_LEN, &count, error))
        return NULL;

    key_extractor = get_extractor_for_object_type(key_type);
    value_extractor = get_extractor_for_object_type(value_type);

    entries = g_new0(GVariant*, count);

    for (int i = 0; i < count; i++) {
        key = key_extractor(pos, end_ptr, error);
        if (!key)
            goto extract_hashtable_object_error;

        value = value_extractor(pos, end_ptr, error);
        if (!value) {
            g_variant_unref(key);
            goto extract_hashtable_object_error;
        }

        entries[i] = g_variant_new_tuple((GVariant*[]) { key, value }, 2);
    }

    variant = g_variant_new_tuple(
        (GVariant*[]) {
            g_variant_new_byte(key_type),
            g_variant_new_byte(value_type),
            g_variant_new_array(G_VARIANT_TYPE_TUPLE, entries, count)
        }, 3);

    g_free(entries);

    return variant;

extract_hashtable_object_error:
    if (entries) {
        for (int i = 0; i < count && entries[i] != NULL; i++)
            g_variant_unref(entries[i]);

        g_free(entries);
    }

    return NULL;
}

static GVariant *
extract_hdata_object(void **pos,
                     const void *end_ptr,
                     GError **error) {
    GVariant *variant = NULL;
    GVariant **hdata_items = NULL,
             **key_info_variants, **hpath_name_variants;
    const GVariantType *key_info_variant_type;
    gchar *hpath = NULL,
          *key_string = NULL;
    gchar **hpath_names = NULL, **key_names = NULL;
    gsize hpath_count, key_count;
    gint32 count;
    struct {
        gchar *name;
        LibWCRelayObjectType type;
    } *key_info;

    hpath = extract_string(pos, end_ptr, OBJECT_HDATA_HPATH_LEN_LEN, FALSE,
                           error);
    if (!hpath)
        return NULL;

    hpath_names = g_strsplit(hpath, "/", -1);
    hpath_count = g_strv_length(hpath_names);

    /* Extract the type information for each of the keys. Since we can expect
     * all of the values in each hdata item to be in order, it's easier to do
     * this in an array */
    key_string = extract_string(pos, end_ptr, OBJECT_HDATA_KEY_STRING_LEN_LEN,
                                FALSE, error);
    if (!key_string)
        goto extract_hdata_object_error;

    key_names = g_strsplit(key_string, ",", -1);
    key_count = g_strv_length(key_names);

    key_info = g_malloc0(sizeof(*key_info) * key_count);

    /* Convert the string containing the key names and their types into the
     * key_info struct we defined earlier, this makes looking up the data type
     * for each key much faster and easier */
    for (int i = 0; key_names[i] != NULL; i++) {
        gchar **split_str = g_strsplit(key_names[i], ":", 2);

        if (g_strv_length(split_str) < 2) {
            gchar *key_type = g_strescape(key_names[i], NULL);

            g_warn_if_reached();

            g_set_error(error, LIBWC_ERROR_RELAY, LIBWC_ERROR_RELAY_INVALID_DATA,
                        "Invalid key:datatype specification in hdata: \"%s\"",
                        key_type);

            g_strfreev(split_str);
            g_free(key_type);

            goto extract_hdata_object_error;
        }

        key_info[i].type = get_object_type_from_string(split_str[1], error);

        if (!key_info[i].type) {
            g_strfreev(split_str);
            goto extract_hdata_object_error;
        }

        key_info[i].name = split_str[0];
        split_str[0] = NULL;

        g_strfreev(split_str);
    }

    if (!extract_size(pos, end_ptr, OBJECT_HDATA_LEN_LEN, &count, error))
        goto extract_hdata_object_error;

    hdata_items = g_new0(GVariant*, count);

    /* Actually parse all of the hdata objects */
    for (int i = 0; i < count; i++) {
        GVariantDict *p_path, *keys;
        LibWCObjectExtractor value_extractor;
        GVariant *value;

        /* First comes the p-path objects */
        p_path = g_variant_dict_new(NULL);
        for (int j = 0; j < hpath_count; j++) {
            value = extract_pointer_object(pos, end_ptr, error);
            if (!value) {
                g_variant_dict_unref(p_path);
                goto extract_hdata_object_error;
            }

            g_variant_dict_insert_value(p_path, hpath_names[j], value);
        }

        /* Next comes the actual objects */
        keys = g_variant_dict_new(NULL);
        for (int j = 0; j < key_count; j++) {
            value_extractor = get_extractor_for_object_type(key_info[j].type);

            value = value_extractor(pos, end_ptr, error);
            if (!value) {
                g_variant_dict_unref(p_path);
                g_variant_dict_unref(keys);
                goto extract_hdata_object_error;
            }

            g_variant_dict_insert_value(keys, key_info[j].name, value);
        }

        /* Pack them in a tuple, inside a variant container. We use the extra
         * container so that the type remains fixed, so we can later put them in
         * an array */
        hdata_items[i] = g_variant_new_variant(
            g_variant_new_tuple((GVariant*[]) {
                g_variant_dict_end(p_path),
                g_variant_dict_end(keys)
            }, 2)
        );
    }

    hpath_name_variants = g_new(GVariant*, hpath_count);
    for (int i = 0; i < hpath_count; i++)
           hpath_name_variants[i] = g_variant_new_take_string(hpath_names[i]);

    /* Take the key_info struct we made before, and convert it into an array we
     * pack with the hdata struct to provide a description of it's layout */
    key_info_variants = g_new(GVariant*, key_count);
    for (int i = 0; i < key_count; i++) {
        key_info_variants[i] = g_variant_new_tuple(
            (GVariant*[]) {
                g_variant_new_take_string(key_info[i].name),
                g_variant_new_byte(key_info[i].type)
            }, 2);
    }

    key_info_variant_type = g_variant_get_type(key_info_variants[0]);

    /* Finally pack everything in a variant that we can return */
    variant = g_variant_new_tuple(
        (GVariant*[]) {
            g_variant_new_array(G_VARIANT_TYPE_STRING, hpath_name_variants,
                                hpath_count),
            g_variant_new_array(key_info_variant_type, key_info_variants,
                                key_count),
            g_variant_new_array(G_VARIANT_TYPE_VARIANT, hdata_items, count)
        }, 3);

    g_free(hpath);
    g_free(key_string);
    /* We don't need to worry about freeing the elements in any of these
     * variables, at this point all of them have been taken by a GVariant of
     * some sort */
    g_free(hpath_names);
    g_free(key_names);
    g_free(key_info);
    g_free(hpath_name_variants);
    g_free(key_info_variants);
    g_free(hdata_items);

    return variant;

extract_hdata_object_error:
    g_strfreev(hpath_names);
    g_strfreev(key_names);

    g_free(hpath);
    g_free(key_string);
    g_free(key_info);

    if (hdata_items) {
        for (int i = 0; i < count && hdata_items[i] != NULL; i++)
            g_variant_unref(hdata_items[i]);

        g_free(hdata_items);
    }

    return NULL;
}

static GVariant *
extract_info_object(void **pos,
                    const void *end_ptr,
                    GError **error) {
    GVariant *variant, *name, *value;

    name = extract_string_object(pos, end_ptr, error);
    if (!name)
        return NULL;

    value = extract_string_object(pos, end_ptr, error);
    if (!value)
        return NULL;

    variant = g_variant_new_tuple((GVariant*[]) { name, value }, 2);

    return variant;
}

static GVariant *
extract_infolist_object(void **pos,
                        const void *end_ptr,
                        GError **error) {
    GVariant *variant,
             *name = NULL,
             *item_name = NULL,
             **items = NULL,
             **variables = NULL;
    gint32 count, variable_count;
    gsize variable_array_size = 0;

    name = extract_string_object(pos, end_ptr, error);
    if (!name)
        goto extract_infolist_object_error;

    if (!extract_size(pos, end_ptr, OBJECT_INFOLIST_LEN_LEN, &count, error))
        goto extract_infolist_object_error;

    items = g_new0(GVariant*, count);

    for (int i = 0; i < count; i++) {
        if (!extract_size(pos, end_ptr, OBJECT_INFOLIST_LEN_LEN, &variable_count,
                          error))
            goto extract_infolist_object_error;

        /* We avoid reallocating the variables struct every time we loop by just
         * making it larger if we need more room in it */
        if (variable_array_size < variable_count) {
            variables = g_renew(GVariant*, variables, variable_count);
            memset(variables, '\0', variable_count);
        }

        for (int i = 0; i < variable_count; i++) {
            LibWCRelayObjectType type;
            LibWCObjectExtractor extractor;
            GVariant *value;

            item_name = extract_string_object(pos, end_ptr, error);
            if (!item_name)
                goto extract_infolist_object_error;

            type = extract_object_type(pos, end_ptr, error);
            if (!type)
                goto extract_infolist_object_error;

            extractor = get_extractor_for_object_type(type);

            value = extractor(pos, end_ptr, error);
            if (!value)
                goto extract_infolist_object_error;

            variables[i] = g_variant_new_variant(
                g_variant_new_tuple((GVariant*[]) {
                    item_name,
                    g_variant_new_byte(type),
                    value
                }, 3));
        }

        items[i] = g_variant_new_array(G_VARIANT_TYPE_VARIANT, variables,
                                       variable_count);
    }

    variant = g_variant_new_tuple(
        (GVariant*[]) {
            name,
            g_variant_new_array(g_variant_get_type(items[0]), items, count)
        }, 2);

    g_free(items);
    g_free(variables);

    return variant;

extract_infolist_object_error:
    if (name)
        g_variant_unref(name);
    if (item_name)
        g_variant_unref(item_name);

    if (items) {
        for (int i = 0; i < count && items[i] != NULL; i++)
            g_variant_unref(items[i]);

        g_free(items);
    }

    if (variables) {
        for (int i = 0; i < variable_count && variables[i] != NULL; i++)
            g_variant_unref(variables[i]);

        g_free(variables);
    }

    return NULL;
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
        case LIBWC_OBJECT_TYPE_HASHTABLE:
            extractor = extract_hashtable_object;
            break;
        case LIBWC_OBJECT_TYPE_HDATA:
            extractor = extract_hdata_object;
            break;
        case LIBWC_OBJECT_TYPE_INFO:
            extractor = extract_info_object;
            break;
        case LIBWC_OBJECT_TYPE_INFOLIST:
            extractor = extract_infolist_object;
            break;
        case LIBWC_OBJECT_TYPE_ARRAY:
            extractor = extract_array_object;
            break;
        default:
            extractor = NULL;
            break;
    }

    return extractor;
} G_GNUC_PURE

static LibWCRelayMessageObject *
extract_object(void **pos,
               const void *end_ptr,
               GError **error) {
    GVariant *variant;
    LibWCRelayMessageObject *object;
    LibWCObjectExtractor extractor;
    LibWCRelayObjectType type;

    type = extract_object_type(pos, end_ptr, error);
    if (!type)
        return NULL;

    extractor = get_extractor_for_object_type(type);
    variant = extractor(pos, end_ptr, error);
    if (!variant)
        return NULL;

    object = g_slice_alloc(sizeof(object));
    object->type = type;
    object->value = variant;

    return object;
}

static GList *
extract_objects(void **pos,
                const void *end_ptr,
                GError **error) {
    LibWCRelayMessageObject *object;
    GList *objects = NULL;

    while (*pos < end_ptr) {
        object = extract_object(pos, end_ptr, error);
        if (!object)
            goto extract_objects_error;

        objects = g_list_prepend(objects, object);
    }

    objects = g_list_reverse(objects);

    return objects;

extract_objects_error:
    if (objects) {
        for (GList *l = objects; l != NULL; l = l->next)
            g_variant_unref(l->data);
    }
    g_warn_if_reached();

    return NULL;
}

LibWCEventIdentifier
extract_event_id(void **pos,
                 const void *end_ptr,
                 GError **error) {
    LibWCEventIdentifier event_id;
    void *new_pos = *pos;
    gchar *identifier_string =
        extract_string(&new_pos, end_ptr, OBJECT_STRING_LEN_LEN, TRUE, error);

    /* If the string is NULL, there's no ID */
    if (!identifier_string) {
        *pos = new_pos;
        return 0;
    }

    event_id = (LibWCEventIdentifier)g_hash_table_lookup(
        event_identifiers, identifier_string);

    /* If we don't recongnize the event identifier, then it's a response
     * identifier and we shouldn't move the pointer forward. Return 0 and let
     * the caller handle the rest
     */
    if (!event_id)
        return LIBWC_NOT_AN_EVENT;

    *pos = new_pos;

    return event_id;
}

LibWCRelayMessage *
libwc_relay_message_parse_data(void *data,
                               gsize size,
                               GError **error) {
    void *pos = data;
    const void *end_ptr = data + size;
    LibWCRelayMessage *message = g_new(LibWCRelayMessage, 1);

    g_assert_null(*error);

    message->id = extract_event_id(&pos, end_ptr, error);
    if (*error)
        goto libwc_relay_message_parse_data_error;

    message->objects = extract_objects(&pos, data + size, error);
    if (!message->objects)
        goto libwc_relay_message_parse_data_error;

    return message;

libwc_relay_message_parse_data_error:
    g_free(message);

    return NULL;
}

static inline void
init_object_type(gchar *str,
                 LibWCRelayObjectType type) {
    g_hash_table_insert(type_identifiers, str, GINT_TO_POINTER(type));
}

static inline void
init_cmd_identifier(gchar *str,
                    LibWCEventIdentifier id) {
    g_hash_table_insert(event_identifiers, str, GINT_TO_POINTER(id));
}

void
_libwc_init_msg_parser() LIBWC_CONSTRUCTOR;

void
_libwc_init_msg_parser() {
    event_identifiers = g_hash_table_new(g_str_hash, g_str_equal);
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

    init_cmd_identifier("_buffer_opened",           LIBWC_EVENT_BUFFER_OPENED);
    init_cmd_identifier("_buffer_type_changed",     LIBWC_EVENT_BUFFER_TYPE_CHANGED);
    init_cmd_identifier("_buffer_moved",            LIBWC_EVENT_BUFFER_MOVED);
    init_cmd_identifier("_buffer_merged",           LIBWC_EVENT_BUFFER_MERGED);
    init_cmd_identifier("_buffer_unmerged",         LIBWC_EVENT_BUFFER_UNMERGED);
    init_cmd_identifier("_buffer_hidden",           LIBWC_EVENT_BUFFER_HIDDEN);
    init_cmd_identifier("_buffer_unhidden",         LIBWC_EVENT_BUFFER_UNHIDDEN);
    init_cmd_identifier("_buffer_renamed",          LIBWC_EVENT_BUFFER_RENAMED);
    init_cmd_identifier("_buffer_title_changed",    LIBWC_EVENT_BUFFER_TITLE_CHANGED);
    init_cmd_identifier("_buffer_localvar_added",   LIBWC_EVENT_BUFFER_LOCALVAR_ADDED);
    init_cmd_identifier("_buffer_localvar_changed", LIBWC_EVENT_BUFFER_LOCALVAR_CHANGED);
    init_cmd_identifier("_buffer_localvar_removed", LIBWC_EVENT_BUFFER_LOCALVAR_REMOVED);
    init_cmd_identifier("_buffer_closing",          LIBWC_EVENT_BUFFER_CLOSING);
    init_cmd_identifier("_buffer_cleared",          LIBWC_EVENT_BUFFER_CLEARED);
    init_cmd_identifier("_buffer_line_added",       LIBWC_EVENT_BUFFER_LINE_ADDED);
    init_cmd_identifier("_nicklist",                LIBWC_EVENT_NICKLIST);
    init_cmd_identifier("_nicklist_diff",           LIBWC_EVENT_NICKLIST_DIFF);
    init_cmd_identifier("_pong",                    LIBWC_EVENT_PONG);
    init_cmd_identifier("_upgrade",                 LIBWC_EVENT_UPGRADE);
    init_cmd_identifier("_upgrade_ended",           LIBWC_EVENT_UPGRADE_ENDED);
}
