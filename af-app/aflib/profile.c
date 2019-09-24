/*
 * load the binary profile to determine the type of each attribute.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "aflib.h"
#include "./codec.h"

#define DEFAULT_PROFILE_FILENAME "/etc/hub.profile"

#define MAX_PROFILE_SIZE (1L << 20)

#define IMAGE_HEADER_LENGTH 216
#define PROFILE_HEADER_LENGTH 6
#define ATTR_HEADER_LENGTH 8

static af_lib_error_t profile_parse(af_profile_t *profile, const uint8_t *buffer, size_t len) {
    size_t index = 0;
    if (len < IMAGE_HEADER_LENGTH + PROFILE_HEADER_LENGTH) return AF_PROFILE_ERROR_CORRUPTED;

    index += IMAGE_HEADER_LENGTH;
    uint16_t version = READ_U16(buffer, index);
    if (version != 2) return AF_PROFILE_ERROR_TOO_NEW;
    uint16_t attribute_count = READ_U16(buffer, index + 4);
    index += PROFILE_HEADER_LENGTH;

    profile->attribute_count = attribute_count;
    profile->attributes = calloc(attribute_count, sizeof(af_attribute_t));

    for (int i = 0; i < attribute_count; i++) {
        // truncated?
        if (index + ATTR_HEADER_LENGTH > len) {
            profile->attribute_count = i;
            break;
        }

        af_attribute_t *attr = &profile->attributes[i];
        attr->attr_id = READ_U16(buffer, index);
        attr->type = READ_U16(buffer, index + 2);
        attr->flags = READ_U16(buffer, index + 4);
        attr->max_length = READ_U16(buffer, index + 6);
        attr->user_data = NULL;
        index += ATTR_HEADER_LENGTH;

        if (attr->flags & ATTR_FLAG_HAS_DEFAULT) {
            uint16_t value_len = READ_U16(buffer, index);
            index += 2 + value_len;
        }
    }

    return AF_PROFILE_ERROR_NONE;
}

static uint8_t *load_file(const char *filename, size_t *len, af_profile_error_t *error) {
    *len = 0;
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        *error = AF_PROFILE_ERROR_FILE_NOT_FOUND;
        return NULL;
    }

    struct stat statbuf;
    if (fstat(fd, &statbuf) < 0) {
        *error = AF_PROFILE_ERROR_FILE_NOT_FOUND;
        goto error;
    }
    if (statbuf.st_size > MAX_PROFILE_SIZE) {
        *error = AF_PROFILE_ERROR_TOO_BIG;
        goto error;
    }

    *len = statbuf.st_size;
    uint8_t *buffer = calloc(1, *len);
    if (buffer == NULL) goto error;
    if (read(fd, buffer, *len) < *len) {
        *error = AF_PROFILE_ERROR_CORRUPTED;
        free(buffer);
        goto error;
    }
    return buffer;

error:
    close(fd);
    *len = 0;
    return NULL;
}

af_profile_error_t aflib_profile_init(af_profile_t *profile) {
    if (profile == NULL) {
        return AF_PROFILE_ERROR_BAD_PARAM;
    }
    profile->attribute_count = 0;
    profile->attributes = NULL;
    return AF_PROFILE_ERROR_NONE;
}

/*
 * parse a binary profile from a file, and fill in a compact table of the
 * attributes, their ids, and types.
 */
af_profile_error_t aflib_profile_load(const char *filename, af_profile_t *profile) {
    if (profile == NULL) {
        return AF_PROFILE_ERROR_BAD_PARAM;
    }

    if (profile->attributes) {
        free(profile->attributes);
        profile->attribute_count = 0;
        profile->attributes = NULL;
    }

    af_profile_error_t error = AF_PROFILE_ERROR_NONE;
    size_t len;
    uint8_t *data = load_file(filename != NULL ? filename : DEFAULT_PROFILE_FILENAME, &len, &error);
    if (error != AF_PROFILE_ERROR_NONE) return error;

    error = profile_parse(profile, data, len);
    free(data);
    return error;
}

af_attribute_t *aflib_profile_find_attribute(af_profile_t *profile, uint16_t attr_id) {
    for (int i = 0; i < profile->attribute_count; i++) {
        if (profile->attributes[i].attr_id == attr_id) return &profile->attributes[i];
    }
    return NULL;
}
