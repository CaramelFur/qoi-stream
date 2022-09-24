#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "qoi-stream.h"

int main() {
  // set fopen buffer to 4mb

  // open ../img.qoi as stream with a buffer size of 4 megabyte
  FILE *fp = fopen("../img.qoi", "rb");
  if (fp == NULL) {
    printf("Failed to open file\n");
    return 1;
  }

  setvbuf(fp, NULL, _IOFBF, 4 * 1024 * 1024);


  // open ../img.out as output stream
  FILE *fp2 = fopen("../img.out", "wb");
  if (fp2 == NULL) {
    printf("Failed to open file\n");
    return 1;
  }

  qois_dec_state state;
  qois_dec_state_init(&state);

  uint8_t *output_buffer = malloc(50000000);
  if (output_buffer == NULL) {
    printf("Failed to allocate memory\n");
    return 1;
  }
  size_t output_buffer_size = 50000000;
  size_t output_buffer_pos = 0;

  // loop over every byte
  uint8_t byte;
  while (fread(&byte, 1, 1, fp) == 1) {
    // decode byte
    if (output_buffer_size >= output_buffer_size - 256) {
      fwrite(output_buffer, 1, output_buffer_pos, fp2);
      output_buffer_pos = 0;
    }

    uint8_t*out = output_buffer + output_buffer_pos;
    size_t out_size = output_buffer_size - output_buffer_pos;

    qois_decode_byte(&state, byte, out, out_size);
  }

  fclose(fp);
  fclose(fp2);

  printf("Done\n");

  return 0;
}