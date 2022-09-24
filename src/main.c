#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#include "qoi-stream.h"

int main(int argc, char **argv)
{
  if (argc < 3)
  {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s <input.qoi> <output> [channels = 3,4]", argv[0]);
    fprintf(stderr, "  %s <input> <output.qoi> <width> <height> <channels = 3,4> <colorspace = 0,1>", argv[0]);

    return 1;
  }

  FILE *input = fopen(argv[1], "rb");
  if (!input)
  {
    fprintf(stderr, "Failed to open input file '%s'", argv[1]);
    return 1;
  }

  FILE *output = fopen(argv[2], "wb");
  if (!output)
  {
    fprintf(stderr, "Failed to open output file '%s'", argv[2]);
    return 1;
  }

  bool input_ends_with_qoi = (strlen(argv[1]) > 4 && strcmp(argv[1] + strlen(argv[1]) - 4, ".qoi") == 0);
  bool output_ends_with_qoi = (strlen(argv[2]) > 4 && strcmp(argv[2] + strlen(argv[2]) - 4, ".qoi") == 0);

  // Refuse if both end in .qoi, or if neither end in .qoi
  if (input_ends_with_qoi == output_ends_with_qoi)
  {
    fprintf(stderr, "Only one of the input and output files may end in .qoi");
    return 1;
  }

  if (input_ends_with_qoi)
  {
    // Check if channels is specified
    uint8_t channels = 0;
    if (argc > 3)
    {
      channels = (uint8_t)atoi(argv[3]);
      if (channels != 3 && channels != 4)
      {
        fprintf(stderr, "Channels override must be 3 or 4");
        return 1;
      }
    }

    // Read file in blocks of 1MB
    const size_t input_buffer_size = 1024 * 1024;
    uint8_t *input_buffer = malloc(input_buffer_size);

    // Write file in blocks of 1MB
    const size_t output_buffer_size = 1024 * 1024;
    uint8_t *output_buffer = malloc(output_buffer_size);
    size_t output_buffer_pos = 0;

    // Read the file in blocks of 1MB and print each byte to stdout
    qois_dec_state state;
    qois_dec_state_init(&state, channels);

    while (true)
    {
      size_t read = fread(input_buffer, 1, input_buffer_size, input);
      if (read == 0)
        break;

      for (size_t i = 0; i < read; i++)
      {

        if (output_buffer_pos >= output_buffer_size - 256)
        {
          fwrite(output_buffer, 1, output_buffer_pos, output);
          output_buffer_pos = 0;
        }

        uint8_t *out = output_buffer + output_buffer_pos;
        size_t out_size = output_buffer_size - output_buffer_pos;

        int outputted = qois_decode_byte(&state, input_buffer[i], out, out_size);
        if (outputted < 0)
        {
          fprintf(stderr, "Failed to decode byte: %d", input_buffer[i]);
          return 1;
        }

        output_buffer_pos += (size_t)outputted;
      }
    }

    if (state.state != QOIS_STATE_DONE)
    {
      fprintf(stderr, "Image ended before decoding was complete");
    }

    fwrite(output_buffer, 1, output_buffer_pos, output);

    printf("Image Info:\n");
    printf("  Width: %d\n", state.desc.width);
    printf("  Height: %d\n", state.desc.height);
    printf("  Channels: %d\n", state.desc.channels);
    printf("  Colorspace: %d\n", state.desc.colorspace);
  }
  else
  {
    // Require 4 more arguments: with, height, channels, and colorspace
    if (argc < 7)
    {
      fprintf(stderr, "Usage: %s <input[.qoi]> <output[.qoi]> [width] [height] [channels] [colorspace]", argv[0]);
      return 1;
    }

    uint32_t width = (uint32_t)atoi(argv[3]);
    uint32_t height = (uint32_t)atoi(argv[4]);
    uint8_t channels = (uint8_t)atoi(argv[5]);
    uint8_t colorspace = (uint8_t)atoi(argv[6]);

    // Read file in blocks of 1MB
    const size_t input_buffer_size = 1024 * 1024;
    uint8_t *input_buffer = malloc(input_buffer_size);

    // Write file in blocks of 1MB
    const size_t output_buffer_size = 1024 * 1024;
    uint8_t *output_buffer = malloc(output_buffer_size);
    size_t output_buffer_pos = 0;

    // Read the file in blocks of 1MB and print each byte to stdout

    qois_enc_state state;
    qois_enc_state_init(&state, width, height, channels, colorspace);

    while (true)
    {
      size_t read = fread(input_buffer, 1, input_buffer_size, input);
      if (read == 0)
        break;

      for (size_t i = 0; i < read; i++)
      {

        if (output_buffer_pos >= output_buffer_size - 256)
        {
          fwrite(output_buffer, 1, output_buffer_pos, output);
          output_buffer_pos = 0;
        }

        uint8_t *out = output_buffer + output_buffer_pos;
        size_t out_size = output_buffer_size - output_buffer_pos;

        int outputted = qois_encode_byte(&state, input_buffer[i], out, out_size);
        if (outputted < 0)
        {
          fprintf(stderr, "Failed to decode byte: %d", input_buffer[i]);
          return 1;
        }

        output_buffer_pos += (size_t)outputted;
      }
    }

    if (state.state != QOIS_STATE_DONE)
    {
      fprintf(stderr, "Data ended before encoding was complete");
    }

    fwrite(output_buffer, 1, output_buffer_pos, output);
  }

  printf("Done\n");

  // Close the files
  fclose(input);
  fclose(output);

  return 0;
}
