#include <stdint.h>
#include <string.h>

// #define SAFE_BUFFER

#ifndef QOIS_STREAM_H
#define QOIS_STREAM_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef SAFE_BUFFER
#define PROGRESS_OUTPUT(value) \
  output += value;
#else
#define PROGRESS_OUTPUT(value) \
  output += value;             \
  output_size -= (size_t)value;
#endif

  // Constants

  const uint8_t qois_magic[4] = {'q', 'o', 'i', 'f'};
  const uint8_t qois_end_magic[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

  // Types

  typedef struct _qois_header
  {
    uint8_t magic[4];
    uint8_t width[4];  // Big endian
    uint8_t height[4]; // Big endian
    uint8_t channels;
    uint8_t colorspace;
  } qois_header;

  typedef struct _qois_desc
  {
    uint32_t width;
    uint32_t height;
    uint8_t channels;
    uint8_t colorspace;
  } qois_desc;

  typedef enum _qois_state
  {
    QOIS_STATE_HEADER = 0,
    QOIS_STATE_FOOTER,
    QOIS_STATE_DONE,

    QOIS_OP_NONE = 10,
    QOIS_OP_RGB,
    QOIS_OP_RGBA,

    QOIS_OP_INDEX,
    QOIS_OP_DIFF,
    QOIS_OP_LUMA,
    QOIS_OP_RUN,
  } qois_state;

  typedef struct _qois_pixel
  {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
  } qois_pixel;

  typedef struct _qois_dec_state
  {
    qois_desc desc;

    qois_state state;
    uint8_t op_data;
    uint8_t op_position;

    size_t pixels_out;
    size_t pixels_count;

    qois_pixel current_pixel;
    qois_pixel last_pixel;

    qois_pixel cache[64];
  } qois_dec_state;

  typedef struct _qois_enc_state
  {
    qois_desc desc;

    qois_state state;
    uint8_t pixel_position;
    uint8_t run_length;

    size_t pixels_in;
    size_t pixels_count;

    qois_pixel current_pixel;
    qois_pixel last_pixel;

    qois_pixel cache[64];
  } qois_enc_state;

  // Util functions

  static inline uint8_t _qois_pixel_cmp(qois_pixel *a, qois_pixel *b)
  {
    return a->r == b->r && a->g == b->g && a->b == b->b && a->a == b->a;
  }

  static inline uint8_t _qois_pixel_hash(qois_pixel *pixel)
  {
    return (uint8_t)((pixel->r * 3 + pixel->g * 5 + pixel->b * 7 + pixel->a * 11) % 64);
  }

  // Init functions

  static inline void _qois_desc_init(qois_desc *desc)
  {
    desc->width = 0;
    desc->height = 0;
    desc->channels = 0;
    desc->colorspace = 0;
  }

  static inline void _qois_pixel_init(qois_pixel *pixel)
  {
    pixel->r = 0;
    pixel->g = 0;
    pixel->b = 0;
    pixel->a = 0xff;
  }

  void qois_enc_state_init(qois_enc_state *state,
                           uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace)

  {
    _qois_desc_init(&state->desc);
    state->desc.width = width;
    state->desc.height = height;
    state->desc.channels = channels;
    state->desc.colorspace = colorspace;

    state->state = QOIS_STATE_HEADER;
    state->run_length = 0;
    state->pixel_position = 0;
    state->pixels_in = 0;
    state->pixels_count = width * height;

    _qois_pixel_init(&state->current_pixel);
    _qois_pixel_init(&state->last_pixel);
    memset(state->cache, 0, sizeof(state->cache));
  }

  void qois_dec_state_init(qois_dec_state *state)
  {
    state->state = QOIS_STATE_HEADER;
    state->op_data = 0;
    state->op_position = 0;
    state->pixels_out = 0;
    state->pixels_count = 0;

    memset(state->cache, 0, sizeof(state->cache));
    _qois_pixel_init(&state->current_pixel);
    _qois_pixel_init(&state->last_pixel);
    _qois_desc_init(&state->desc);
  }

  // Encode functions

  static inline int _qois_encode_header(qois_enc_state *state, uint8_t *output, size_t output_size)
  {
#ifdef SAFE_BUFFER
    if (output_size < sizeof(qois_header))
      return -1;
#endif
    (void)output_size;

    output[0] = qois_magic[0];
    output[1] = qois_magic[1];
    output[2] = qois_magic[2];
    output[3] = qois_magic[3];

    output[4] = (uint8_t)(state->desc.width >> 24) & 0xff;
    output[5] = (uint8_t)(state->desc.width >> 16) & 0xff;
    output[6] = (uint8_t)(state->desc.width >> 8) & 0xff;
    output[7] = (uint8_t)(state->desc.width >> 0) & 0xff;

    output[8] = (uint8_t)(state->desc.height >> 24) & 0xff;
    output[9] = (uint8_t)(state->desc.height >> 16) & 0xff;
    output[10] = (uint8_t)(state->desc.height >> 8) & 0xff;
    output[11] = (uint8_t)(state->desc.height >> 0) & 0xff;

    output[12] = state->desc.channels;
    output[13] = state->desc.colorspace;
    return sizeof(qois_header);
  }

  static inline int _qois_encode_pixel_byte(qois_enc_state *state, uint8_t byte, uint8_t *output, size_t output_size)
  {
    // Put the byte in the current pixel in RGBA order
    ((uint8_t *)(&state->current_pixel))[state->pixel_position] = byte;
    state->pixel_position++;

    if (state->pixel_position < state->desc.channels)
      return 0;
    state->pixel_position = 0;
    state->pixels_in++;

    int outputted = 0;

    // RUN LENGTH

    uint8_t finish_run_length_immediately = 0;
    if (_qois_pixel_cmp(&state->current_pixel, &state->last_pixel))
    {
      state->run_length++;
      if (state->run_length < 62 && state->pixels_in < state->pixels_count)
        return 0;
      finish_run_length_immediately = 1;
    }

    if (state->run_length > 0)
    {
#ifdef SAFE_BUFFER
      if (output_size < 1)
        return -1;
#endif

      if (state->run_length == 1)
      {
        uint8_t hash = _qois_pixel_hash(&state->last_pixel);
        output[0] = 0b00000000 | hash;
      }
      else
      {
        output[0] = 0b11000000 | (state->run_length - 1);
      }

      state->run_length = 0;
      outputted += 1;
      PROGRESS_OUTPUT(1);

      // Can only come here if this pixel is also a run length pixel
      if (finish_run_length_immediately)
        goto finish_pixel;
    }

    // INDEX

    {
      uint8_t hash = _qois_pixel_hash(&state->current_pixel);
      if (_qois_pixel_cmp(&state->current_pixel, &state->cache[hash]))
      {
#ifdef SAFE_BUFFER
        if (output_size < 1)
          return -1;
#endif

        output[0] = 0b00000000 | hash;
        outputted += 1;
        goto finish_pixel;
      }
    }

    // RGBA

    if (state->desc.channels > 3 && state->current_pixel.a != state->last_pixel.a)
    {
#ifdef SAFE_BUFFER
      if (output_size < 5)
        return -1;
#endif

      output[0] = 0b11111111;
      output[1] = state->current_pixel.r;
      output[2] = state->current_pixel.g;
      output[3] = state->current_pixel.b;
      output[4] = state->current_pixel.a;

      outputted += 5;
      goto finish_pixel;
    }

    {
      // DIFF

      int8_t red_diff = (int8_t)(state->current_pixel.r - state->last_pixel.r);
      int8_t green_diff = (int8_t)(state->current_pixel.g - state->last_pixel.g);
      int8_t blue_diff = (int8_t)(state->current_pixel.b - state->last_pixel.b);

      if (
          red_diff <= 1 && red_diff >= -2 &&
          green_diff <= 1 && green_diff >= -2 &&
          blue_diff <= 1 && blue_diff >= -2)
      {
#ifdef SAFE_BUFFER
        if (output_size < 1)
          return -1;
#endif

        output[0] = (uint8_t)0b01000000 |
                    (uint8_t)((red_diff + 2) << 4) |
                    (uint8_t)((green_diff + 2) << 2) |
                    (uint8_t)((blue_diff + 2) << 0);

        outputted += 1;
        goto finish_pixel;
      }

      // LUMA

      int8_t dr_dg = (int8_t)(red_diff - green_diff);
      int8_t db_dg = (int8_t)(blue_diff - green_diff);

      if (
          dr_dg >= -8 && dr_dg <= 7 &&
          green_diff >= -32 && green_diff <= 31 &&
          db_dg >= -8 && db_dg <= 7)
      {
#ifdef SAFE_BUFFER
        if (output_size < 2)
          return -1;
#endif

        output[0] = 0b10000000 | (uint8_t)(green_diff + 32);
        output[1] = ((uint8_t)(dr_dg + 8) << 4) | ((uint8_t)(db_dg + 8) << 0);

        outputted += 2;
        goto finish_pixel;
      }
    }

    // RGB

    {
#ifdef SAFE_BUFFER
      if (output_size < 4)
        return -1;
#endif

      output[0] = 0b11111110;
      output[1] = state->current_pixel.r;
      output[2] = state->current_pixel.g;
      output[3] = state->current_pixel.b;

      outputted += 4;
      goto finish_pixel;
    }

    {
    finish_pixel:

      uint8_t hash = _qois_pixel_hash(&state->current_pixel);
      state->cache[hash] = state->current_pixel;
      state->last_pixel = state->current_pixel;
    }

    return outputted;
  }

  static inline int qois_encode_byte(qois_enc_state *state, uint8_t byte, uint8_t *output, size_t output_size)
  {
    int outputted = 0;

    if (state->state == QOIS_STATE_HEADER)
    {
      int result = _qois_encode_header(state, output, output_size);
      if (result < 0)
        return result;

      outputted += result;
      PROGRESS_OUTPUT(result);

      state->state = QOIS_OP_NONE;
    }

    if (state->state >= QOIS_OP_NONE)
    {
      int result = _qois_encode_pixel_byte(state, byte, output, output_size);
      if (result < 0)
        return result;

      outputted += result;
      PROGRESS_OUTPUT(result);

      if (state->pixels_in == state->pixels_count)
        state->state = QOIS_STATE_FOOTER;
    }

    if (state->state == QOIS_STATE_FOOTER)
    {
#ifdef SAFE_BUFFER
      if (output_size < 8)
        return -1;
#endif

      memcpy(output, qois_end_magic, 8);

      state->state = QOIS_STATE_DONE;
      outputted += 8;
    }

    return outputted;
  }

  // Decode functions

  static inline int _qois_decode_header_byte(qois_dec_state *state, uint8_t byte)
  {
    switch (state->op_position)
    {
    case 0:
      if (byte != qois_magic[0])
        return -1;
      break;
    case 1:
      if (byte != qois_magic[1])
        return -1;
      break;
    case 2:
      if (byte != qois_magic[2])
        return -1;
      break;
    case 3:
      if (byte != qois_magic[3])
        return -1;
      break;
    case 4:
      state->desc.width |= byte << 24;
      break;
    case 5:
      state->desc.width |= byte << 16;
      break;
    case 6:
      state->desc.width |= byte << 8;
      break;
    case 7:
      state->desc.width |= byte;
      break;
    case 8:
      state->desc.height |= byte << 24;
      break;
    case 9:
      state->desc.height |= byte << 16;
      break;
    case 10:
      state->desc.height |= byte << 8;
      break;
    case 11:
      state->desc.height |= byte;
      break;
    case 12:
      state->desc.channels = byte;

      state->pixels_count = state->desc.width * state->desc.height;
      if (state->desc.channels != 3 && state->desc.channels != 4)
        return -1;
      break;
    case 13:
      state->desc.colorspace = byte;
      if (state->desc.colorspace != 0 && state->desc.colorspace != 1)
        return -1;
      break;
    default:
      return -1;
    }

    state->op_position++;
    return 0;
  }

  static inline int _qois_decode_footer_byte(qois_dec_state *state, uint8_t byte)
  {
    if (byte != qois_end_magic[state->op_position])
      return -1;
    state->op_position++;
    return 0;
  }

  static inline qois_state _qois_parse_op(uint8_t opcode)
  {
    if (opcode == 0b11111111)
      return QOIS_OP_RGBA;
    if (opcode == 0b11111110)
      return QOIS_OP_RGB;
    if (opcode <= 0b00111111)
      return QOIS_OP_INDEX;
    if (opcode <= 0b01111111)
      return QOIS_OP_DIFF;
    if (opcode <= 0b10111111)
      return QOIS_OP_LUMA;
    if (opcode <= 0b11111101)
      return QOIS_OP_RUN;
    return QOIS_OP_NONE;
  }

  static inline int _qois_decode_copy_current_pixel(qois_dec_state *state, uint8_t *output, size_t output_size, size_t offset)
  {
    offset *= state->desc.channels;

#ifdef SAFE_BUFFER
    if (offset + state->desc.channels > output_size)
      return -1;
#endif

    PROGRESS_OUTPUT(offset);

    memcpy(output, &state->current_pixel, state->desc.channels);

    uint8_t hash = _qois_pixel_hash(&state->current_pixel);
    state->cache[hash] = state->current_pixel;

    return 1;
  }

  static inline int _qois_decode_copy_current_pixel_n(qois_dec_state *state, uint8_t *output, size_t output_size, size_t offset, size_t count)
  {
#ifdef SAFE_BUFFER
    if ((offset + count) * state->desc.channels > output_size)
      return -1;
#endif

    uint8_t channels = state->desc.channels;

    output += offset * state->desc.channels;
    uint8_t *output_end = output + count * state->desc.channels;

    qois_pixel *current_pixel = &state->current_pixel;
    for (; output < output_end; output += channels)
    {
      memcpy(output + offset, current_pixel, channels);
    }

    uint8_t hash = _qois_pixel_hash(&state->current_pixel);
    state->cache[hash] = state->current_pixel;

    return 1;
  }

  static inline int _qois_decode_op_byte(qois_dec_state *state, uint8_t byte, uint8_t *output, size_t output_size)
  {
    uint32_t outputted = 0;
    if (state->state == QOIS_OP_NONE)
    {
      state->last_pixel = state->current_pixel;
      _qois_pixel_init(&state->current_pixel);
      state->state = _qois_parse_op(byte);
      state->op_data = byte & 0b00111111;
      state->op_position = 0;
    }

    switch (state->state)
    {
    case QOIS_OP_RGB:
    {
      if (state->op_position == 1)
        state->current_pixel.r = byte;
      else if (state->op_position == 2)
        state->current_pixel.g = byte;
      else if (state->op_position == 3)
      {
        state->current_pixel.b = byte;

        state->state = QOIS_OP_NONE;

        if (_qois_decode_copy_current_pixel(state, output, output_size, 0) < 0)
          return -1;
        outputted++;
      }
      else if (state->op_position >= 4)
        return -1;
    }
    break;

    case QOIS_OP_RGBA:
    {
      if (state->op_position == 1)
        state->current_pixel.r = byte;
      else if (state->op_position == 2)
        state->current_pixel.g = byte;
      else if (state->op_position == 3)
        state->current_pixel.b = byte;
      else if (state->op_position == 4)
      {
        state->current_pixel.a = byte;

        state->state = QOIS_OP_NONE;

        if (_qois_decode_copy_current_pixel(state, output, output_size, 0) < 0)
          return -1;
        outputted++;
      }
      else if (state->op_position >= 5)
        return -1;
    }
    break;

    case QOIS_OP_INDEX:
    {
      state->current_pixel = state->cache[state->op_data];
      state->state = QOIS_OP_NONE;

      if (_qois_decode_copy_current_pixel(state, output, output_size, 0) < 0)
        return -1;
      outputted++;
    }
    break;

    case QOIS_OP_DIFF:
    {
      uint8_t dr = ((state->op_data >> 4) & 0b00000011) - 2;
      uint8_t dg = ((state->op_data >> 2) & 0b00000011) - 2;
      uint8_t db = ((state->op_data >> 0) & 0b00000011) - 2;

      state->current_pixel.r = state->last_pixel.r + dr;
      state->current_pixel.g = state->last_pixel.g + dg;
      state->current_pixel.b = state->last_pixel.b + db;

      state->state = QOIS_OP_NONE;

      if (_qois_decode_copy_current_pixel(state, output, output_size, 0) < 0)
        return -1;
      outputted++;
    }
    break;

    case QOIS_OP_LUMA:
    {
      if (state->op_position == 1)
      {
        uint8_t diff_green = state->op_data - 32;
        uint8_t diff_red = (byte >> 4) - 8;
        uint8_t diff_blue = (byte & 0b00001111) - 8;

        state->current_pixel.r = state->last_pixel.r + (uint8_t)(diff_green + diff_red);
        state->current_pixel.g = state->last_pixel.g + diff_green;
        state->current_pixel.b = state->last_pixel.b + (uint8_t)(diff_green + diff_blue);

        state->state = QOIS_OP_NONE;

        if (_qois_decode_copy_current_pixel(state, output, output_size, 0) < 0)
          return -1;
        outputted++;
      }
      else if (state->op_position > 1)
        return -1;
    }
    break;

    case QOIS_OP_RUN:
    {
      state->current_pixel = state->last_pixel;

      uint8_t length = state->op_data + 1;
      if (_qois_decode_copy_current_pixel_n(state, output, output_size, 0, length) < 0)
        return -1;

      outputted += length;
      state->state = QOIS_OP_NONE;
    }
    break;

    default:
      return -1;
    }

    state->op_position++;
    state->pixels_out += outputted;
    return (int)outputted * (int)state->desc.channels;
  }

  static inline int qois_decode_byte(qois_dec_state *state, uint8_t byte, uint8_t *output, size_t output_size)
  {
#ifdef SAFE_BUFFER
    if (output_size < state->desc.channels * 64)
      return -1;
#endif

    if (state->state >= QOIS_OP_NONE)
    {
      int outputted = _qois_decode_op_byte(state, byte, output, output_size);

      if (state->pixels_out >= state->pixels_count)
      {
        state->state = QOIS_STATE_FOOTER;
        state->op_position = 0;
      }
      return outputted;
    }
    else if (state->state == QOIS_STATE_HEADER)
    {
      if (_qois_decode_header_byte(state, byte) == -1)
        return -1;

      if (state->op_position == sizeof(qois_header))
        state->state = QOIS_OP_NONE;
    }
    else if (state->state == QOIS_STATE_FOOTER)
    {
      if (_qois_decode_footer_byte(state, byte) == -1)
        return -1;

      if (state->op_position == sizeof(qois_end_magic))
        state->state = QOIS_STATE_DONE;
    }
    else if (state->state == QOIS_STATE_DONE)
      return 0;
    else
      return -1;

    return 0;
  }

#ifdef __cplusplus
}
#endif

#endif