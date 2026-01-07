#include <assert.h>
#include <bare.h>
#include <js.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// BMP file header (14 bytes)
typedef struct __attribute__((packed)) {
  uint16_t magic;       // 'BM' = 0x4D42
  uint32_t file_size;   // Total file size
  uint16_t reserved1;   // Reserved
  uint16_t reserved2;   // Reserved
  uint32_t data_offset; // Offset to pixel data
} bmp_file_header_t;

// DIB header - BITMAPINFOHEADER (40 bytes)
typedef struct __attribute__((packed)) {
  uint32_t header_size;     // Header size (40)
  int32_t width;            // Image width
  int32_t height;           // Image height (negative = top-down)
  uint16_t planes;          // Color planes (always 1)
  uint16_t bpp;             // Bits per pixel (24 or 32)
  uint32_t compression;     // Compression (0 = none)
  uint32_t image_size;      // Image size (can be 0 for uncompressed)
  int32_t x_pixels_per_m;   // Horizontal resolution
  int32_t y_pixels_per_m;   // Vertical resolution
  uint32_t colors_used;     // Colors in palette (0 = all)
  uint32_t colors_important;// Important colors (0 = all)
} bmp_dib_header_t;

static void
bare_bmp__on_finalize(js_env_t *env, void *data, void *finalize_hint) {
  free(data);
}

/**
 * Decode BMP buffer to RGBA format
 * Handles 24-bit BGR and 32-bit BGRA formats
 * Supports both top-down and bottom-up orientations
 */
static js_value_t *
bare_bmp_decode(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 1;
  js_value_t *argv[1];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);
  assert(argc == 1);

  uint8_t *bmp_data;
  size_t bmp_len;
  err = js_get_typedarray_info(env, argv[0], NULL, (void **) &bmp_data, &bmp_len, NULL, NULL);
  assert(err == 0);

  // Validate minimum size
  if (bmp_len < sizeof(bmp_file_header_t) + sizeof(bmp_dib_header_t)) {
    err = js_throw_error(env, NULL, "Invalid BMP: file too small");
    assert(err == 0);
    return NULL;
  }

  // Parse headers
  bmp_file_header_t *file_header = (bmp_file_header_t *) bmp_data;
  bmp_dib_header_t *dib_header = (bmp_dib_header_t *) (bmp_data + sizeof(bmp_file_header_t));

  // Validate magic number
  if (file_header->magic != 0x4D42) {
    err = js_throw_error(env, NULL, "Invalid BMP: wrong magic number");
    assert(err == 0);
    return NULL;
  }

  // Validate DIB header size
  if (dib_header->header_size != 40) {
    err = js_throw_error(env, NULL, "Unsupported BMP: only BITMAPINFOHEADER supported");
    assert(err == 0);
    return NULL;
  }

  // Validate compression
  if (dib_header->compression != 0) {
    err = js_throw_error(env, NULL, "Unsupported BMP: only uncompressed format supported");
    assert(err == 0);
    return NULL;
  }

  // Validate bits per pixel
  if (dib_header->bpp != 24 && dib_header->bpp != 32) {
    err = js_throw_error(env, NULL, "Unsupported BMP: only 24-bit and 32-bit formats supported");
    assert(err == 0);
    return NULL;
  }

  int32_t width = dib_header->width;
  int32_t height = dib_header->height;
  int32_t abs_height = height < 0 ? -height : height;
  bool top_down = height < 0;
  uint16_t bpp = dib_header->bpp;
  uint32_t bytes_per_pixel = bpp / 8;

  // Calculate row size with 4-byte padding
  uint32_t row_size = ((width * bytes_per_pixel + 3) / 4) * 4;

  // Validate data offset and size
  if (file_header->data_offset + (row_size * abs_height) > bmp_len) {
    err = js_throw_error(env, NULL, "Invalid BMP: pixel data exceeds file size");
    assert(err == 0);
    return NULL;
  }

  // Allocate RGBA output buffer
  uint8_t *rgba_data = malloc(width * abs_height * 4);
  if (!rgba_data) {
    err = js_throw_error(env, NULL, "Memory allocation failed");
    assert(err == 0);
    return NULL;
  }

  uint8_t *pixel_data = bmp_data + file_header->data_offset;

  // Convert BGR(A) to RGBA
  for (int32_t y = 0; y < abs_height; y++) {
    // BMP stores pixels bottom-up by default (unless height is negative)
    int32_t src_row = top_down ? y : (abs_height - 1 - y);
    uint8_t *src = pixel_data + src_row * row_size;
    uint8_t *dst = rgba_data + y * width * 4;

    for (int32_t x = 0; x < width; x++) {
      // BGR(A) -> RGBA conversion
      dst[0] = src[2]; // R
      dst[1] = src[1]; // G
      dst[2] = src[0]; // B
      dst[3] = (bpp == 32) ? src[3] : 0xFF; // A

      src += bytes_per_pixel;
      dst += 4;
    }
  }

  // Create result object
  js_value_t *result;
  err = js_create_object(env, &result);
  assert(err == 0);

  // Set width property
  js_value_t *width_val;
  err = js_create_int64(env, width, &width_val);
  assert(err == 0);
  err = js_set_named_property(env, result, "width", width_val);
  assert(err == 0);

  // Set height property
  js_value_t *height_val;
  err = js_create_int64(env, abs_height, &height_val);
  assert(err == 0);
  err = js_set_named_property(env, result, "height", height_val);
  assert(err == 0);

  // Set data property (external ArrayBuffer with finalizer)
  js_value_t *buffer;
  err = js_create_external_arraybuffer(env, rgba_data, width * abs_height * 4, bare_bmp__on_finalize, NULL, &buffer);
  assert(err == 0);
  err = js_set_named_property(env, result, "data", buffer);
  assert(err == 0);

  return result;
}

/**
 * Encode RGBA data to BMP format (24-bit BGR)
 * Always outputs bottom-up format (standard BMP)
 */
static js_value_t *
bare_bmp_encode(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 2;
  js_value_t *argv[2];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);
  assert(argc == 2);

  // Get RGBA object {width, height, data}
  js_value_t *rgba_obj = argv[0];

  // Get width
  js_value_t *width_val;
  err = js_get_named_property(env, rgba_obj, "width", &width_val);
  assert(err == 0);
  int64_t width;
  err = js_get_value_int64(env, width_val, &width);
  assert(err == 0);

  // Get height
  js_value_t *height_val;
  err = js_get_named_property(env, rgba_obj, "height", &height_val);
  assert(err == 0);
  int64_t height;
  err = js_get_value_int64(env, height_val, &height);
  assert(err == 0);

  // Get data
  js_value_t *data_val;
  err = js_get_named_property(env, rgba_obj, "data", &data_val);
  assert(err == 0);
  uint8_t *rgba_data;
  size_t rgba_len;
  err = js_get_typedarray_info(env, data_val, NULL, (void **) &rgba_data, &rgba_len, NULL, NULL);
  assert(err == 0);

  // Validate input
  if (rgba_len < (size_t)(width * height * 4)) {
    err = js_throw_error(env, NULL, "Invalid RGBA: data buffer too small");
    assert(err == 0);
    return NULL;
  }

  // Calculate row size with 4-byte padding (24-bit = 3 bytes per pixel)
  uint32_t bytes_per_pixel = 3;
  uint32_t row_size = ((width * bytes_per_pixel + 3) / 4) * 4;
  uint32_t pixel_data_size = row_size * height;
  uint32_t file_size = sizeof(bmp_file_header_t) + sizeof(bmp_dib_header_t) + pixel_data_size;

  // Allocate output buffer
  uint8_t *bmp_data = malloc(file_size);
  if (!bmp_data) {
    err = js_throw_error(env, NULL, "Memory allocation failed");
    assert(err == 0);
    return NULL;
  }

  memset(bmp_data, 0, file_size);

  // Create file header
  bmp_file_header_t *file_header = (bmp_file_header_t *) bmp_data;
  file_header->magic = 0x4D42; // 'BM'
  file_header->file_size = file_size;
  file_header->reserved1 = 0;
  file_header->reserved2 = 0;
  file_header->data_offset = sizeof(bmp_file_header_t) + sizeof(bmp_dib_header_t);

  // Create DIB header
  bmp_dib_header_t *dib_header = (bmp_dib_header_t *) (bmp_data + sizeof(bmp_file_header_t));
  dib_header->header_size = 40;
  dib_header->width = width;
  dib_header->height = height; // Positive = bottom-up
  dib_header->planes = 1;
  dib_header->bpp = 24;
  dib_header->compression = 0; // No compression
  dib_header->image_size = pixel_data_size;
  dib_header->x_pixels_per_m = 2835; // 72 DPI
  dib_header->y_pixels_per_m = 2835; // 72 DPI
  dib_header->colors_used = 0;
  dib_header->colors_important = 0;

  // Convert RGBA to BGR and write bottom-up
  uint8_t *pixel_data = bmp_data + file_header->data_offset;

  for (int32_t y = 0; y < height; y++) {
    // Write bottom-up (BMP standard)
    int32_t dst_row = height - 1 - y;
    uint8_t *src = rgba_data + y * width * 4;
    uint8_t *dst = pixel_data + dst_row * row_size;

    for (int32_t x = 0; x < width; x++) {
      // RGBA -> BGR conversion (skip alpha)
      dst[0] = src[2]; // B
      dst[1] = src[1]; // G
      dst[2] = src[0]; // R

      src += 4;
      dst += 3;
    }
    // Row padding is already zeroed by memset
  }

  // Create external ArrayBuffer with finalizer
  js_value_t *result;
  err = js_create_external_arraybuffer(env, bmp_data, file_size, bare_bmp__on_finalize, NULL, &result);
  assert(err == 0);

  return result;
}

/**
 * BMP format does not support animation
 */
static js_value_t *
bare_bmp_encode_animated(js_env_t *env, js_callback_info_t *info) {
  int err = js_throw_error(env, NULL, "BMP format does not support animation");
  assert(err == 0);
  return NULL;
}

static js_value_t *
bare_bmp_exports(js_env_t *env, js_value_t *exports) {
  int err;

#define V(name, fn) \
  { \
    js_value_t *val; \
    err = js_create_function(env, name, -1, fn, NULL, &val); \
    assert(err == 0); \
    err = js_set_named_property(env, exports, name, val); \
    assert(err == 0); \
  }

  V("decode", bare_bmp_decode)
  V("encode", bare_bmp_encode)
  V("encodeAnimated", bare_bmp_encode_animated)
#undef V

  return exports;
}

BARE_MODULE(bare_bmp, bare_bmp_exports)
