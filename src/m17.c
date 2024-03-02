/*-------------------------------------------------------------------------------
 * m17.c
 * M17 Decoder and Decoder (LSF and STR)
 *
 * m17_scramble Bit Array from SDR++
 * CRC16 from M17-Implementations (thanks again, sp5wwp)
 *
 * LWVMOBILE
 * 2024-03 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/
#include "dsd.h"

//try to find a fancy lfsr or calculation for this and not an array if possible
uint8_t m17_scramble[369] = { 
1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1,
1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0,
1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0,
1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 0,
1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0,
1, 1, 0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0,
1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1,
0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0,
0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1,
1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1,
1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 0,
0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1,
0, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 0,
0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0, 1, 0, 1, 0,
1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0,
0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1,
1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0,
1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1,
1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1,
0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0,
0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1,
0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1 
};

char b40[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/.";

uint8_t p1[62] = {
1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1,
1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1,
1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1,
1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1
};

//from M17_Implementations -- sp5wwp -- should have just looked here to begin with
//this setup looks very similar to the OP25 variant of crc16, but with a few differences (uses packed bytes)
uint16_t crc16m17(const uint8_t *in, const uint16_t len)
{
  uint32_t crc = 0xFFFF; //init val
  uint16_t poly = 0x5935;
  for(uint16_t i=0; i<len; i++)
  {
    crc^=in[i]<<8;
    for(uint8_t j=0; j<8; j++)
    {
      crc<<=1;
      if(crc&0x10000)
        crc=(crc^poly)&0xFFFF;
    }
  }

  return crc&(0xFFFF);
}

void M17decodeCSD(dsd_state * state, unsigned long long int dst, unsigned long long int src)
{
  //evaluate dst and src, and determine if they need to be converted to calsign
  int i;
  char c;

  if (dst == 0xFFFFFFFFFFFF) 
    fprintf (stderr, " DST: BROADCAST");
  else if (dst == 0)
    fprintf (stderr, " DST: RESERVED %012llx", dst);
  else if (dst >= 0xEE6B28000000)
    fprintf (stderr, " DST: RESERVED %012llx", dst);
  else
  {
    fprintf (stderr, " DST: ");
    for (i = 0; i < 9; i++)
    {
      c = b40[dst % 40];
      state->m17_dst_csd[i] = c;
      fprintf (stderr, "%c", c);
      dst = dst / 40;
    }
    //assign completed CSD to a more useful string instead
    sprintf (state->m17_dst_str, "%c%c%c%c%c%c%c%c%c", 
    state->m17_dst_csd[0], state->m17_dst_csd[1], state->m17_dst_csd[2], state->m17_dst_csd[3], 
    state->m17_dst_csd[4], state->m17_dst_csd[5], state->m17_dst_csd[6], state->m17_dst_csd[7], state->m17_dst_csd[8]);

    //debug
    // fprintf (stderr, "DT: %s", state->m17_dst_str);
  }

  if (src == 0xFFFFFFFFFFFF) 
    fprintf (stderr, " SRC:  UNKNOWN FFFFFFFFFFFF");
  else if (src == 0)
    fprintf (stderr, " SRC: RESERVED %012llx", src);
  else if (src >= 0xEE6B28000000)
    fprintf (stderr, " SRC: RESERVED %012llx", src);
  else
  {
    fprintf (stderr, " SRC: ");
    for (i = 0; i < 9; i++)
    {
      c = b40[src % 40];
      state->m17_src_csd[i] = c;
      fprintf (stderr, "%c", c);
      src = src / 40;
    }
    //assign completed CSD to a more useful string instead
    sprintf (state->m17_src_str, "%c%c%c%c%c%c%c%c%c", 
    state->m17_src_csd[0], state->m17_src_csd[1], state->m17_src_csd[2], state->m17_src_csd[3], 
    state->m17_src_csd[4], state->m17_src_csd[5], state->m17_src_csd[6], state->m17_src_csd[7], state->m17_src_csd[8]);

    //debug
    // fprintf (stderr, "ST: %s", state->m17_src_str);
  }

  //debug
  // fprintf (stderr, " DST: %012llX SRC: %012llX", state->m17_dst, state->m17_src);

}

void M17decodeLSF(dsd_state * state)
{
  int i;
  
  unsigned long long int lsf_dst = (unsigned long long int)ConvertBitIntoBytes(&state->m17_lsf[0], 48);
  unsigned long long int lsf_src = (unsigned long long int)ConvertBitIntoBytes(&state->m17_lsf[48], 48);
  uint16_t lsf_type = (uint16_t)ConvertBitIntoBytes(&state->m17_lsf[96], 16);

  //this is the way the manual/code expects you to read these bits
  // uint8_t lsf_ps = (lsf_type >> 0) & 0x1;
  uint8_t lsf_dt = (lsf_type >> 1) & 0x3;
  uint8_t lsf_et = (lsf_type >> 3) & 0x3;
  uint8_t lsf_es = (lsf_type >> 5) & 0x3;
  uint8_t lsf_cn = (lsf_type >> 7) & 0xF;
  uint8_t lsf_rs = (lsf_type >> 11) & 0x1F;

  //store this so we can reference it for playing voice and/or decoding data, dst/src etc
  state->m17_str_dt = lsf_dt;
  state->m17_dst = lsf_dst;
  state->m17_src = lsf_src;
  state->m17_can = lsf_cn;

  fprintf (stderr, "\n");

  //packet or stream
  // if (lsf_ps == 0) fprintf (stderr, " P-");
  // if (lsf_ps == 1) fprintf (stderr, " S-");

  fprintf (stderr, " CAN: %d", lsf_cn);
  M17decodeCSD(state, lsf_dst, lsf_src);
  
  if (lsf_dt == 0) fprintf (stderr, " Reserved");
  if (lsf_dt == 1) fprintf (stderr, " Data");
  if (lsf_dt == 2) fprintf (stderr, " Voice (3200bps)");
  if (lsf_dt == 3) fprintf (stderr, " Voice + Data");

  if (lsf_rs != 0) fprintf (stderr, " RS: %02X", lsf_rs);

  if (lsf_et != 0) fprintf (stderr, " ENC:");
  if (lsf_et == 2) fprintf (stderr, " AES-CTR");
  if (lsf_et == 1) fprintf (stderr, " Scrambler - %d", lsf_es);

  state->m17_enc = lsf_et;
  state->m17_enc_st = lsf_es;

  //pack meta bits into 14 bytes
  for (i = 0; i < 14; i++)
    state->m17_meta[i] = (uint8_t)ConvertBitIntoBytes(&state->m17_lsf[(i*8)+112], 8);

  //Examine the Meta data
  if (lsf_et == 0 && state->m17_meta[0] != 0) //not sure if this applies universally, or just to text data byte 0 for null data
  {
    fprintf (stderr,  " Meta:");
    if (lsf_es == 0) fprintf (stderr, " Text Data");
    if (lsf_es == 1) fprintf (stderr, " GNSS Pos");
    if (lsf_es == 2) fprintf (stderr, " Extended CSD");
    if (lsf_es == 3) fprintf (stderr, " Reserved");
  }
  
  if (lsf_et == 2)
  {
    fprintf (stderr, " IV: ");
    for (i = 0; i < 16; i++)
      fprintf (stderr, "%02X", state->m17_meta[i]);
  }
  
}

int M17processLICH(dsd_state * state, dsd_opts * opts, uint8_t * lich_bits)
{
  int i, j, err;
  err = 0;

  uint8_t lich[4][24];
  uint8_t lich_decoded[48];
  uint8_t temp[96];
  bool g[4];

  uint8_t lich_counter = 0;
  uint8_t lich_reserve = 0;

  uint16_t crc_cmp = 0;
  uint16_t crc_ext = 0;
  uint8_t crc_err = 0;

  memset(lich, 0, sizeof(lich));
  memset(lich_decoded, 0, sizeof(lich_decoded));
  memset(temp, 0, sizeof(temp));

  //execute golay 24,12 or 4 24-bit chunks and reorder into 4 12-bit chunks
  for (i = 0; i < 4; i++)
  {
    g[i] = TRUE;

    for (j = 0; j < 24; j++)
      lich[i][j] = lich_bits[(i*24)+j];

    g[i] = Golay_24_12_decode(lich[i]);
    if(g[i] == FALSE) err = -1;

    for (j = 0; j < 12; j++)
      lich_decoded[i*12+j] = lich[i][j];

  }

  lich_counter = (uint8_t)ConvertBitIntoBytes(&lich_decoded[40], 3); //lich_cnt
  lich_reserve = (uint8_t)ConvertBitIntoBytes(&lich_decoded[43], 5); //lich_reserved

  if (err == 0)
    fprintf (stderr, "LC: %d/6 ", lich_counter+1);
  else fprintf (stderr, "LICH G24 ERR");

  // if (err == 0 && lich_reserve != 0) fprintf(stderr, " LRS: %d", lich_reserve);

  //This is not M17 standard, but use the LICH reserved bits to signal data type and CAN value
  if (err == 0 && opts->m17encoder == 1) //only use when using built in encoder
  {
    state->m17_str_dt = lich_reserve & 0x3;
    state->m17_can = (lich_reserve >> 2) & 0x7;
  }

  //transfer to storage
  for (i = 0; i < 40; i++)
    state->m17_lsf[lich_counter*40+i] = lich_decoded[i];

  if (opts->payload == 1)
  {
    fprintf (stderr, " LICH: ");
    for (i = 0; i < 6; i++)
      fprintf (stderr, "[%02X]", (uint8_t)ConvertBitIntoBytes(&lich_decoded[i*8], 8)); 
  }

  uint8_t lsf_packed[30];
  memset (lsf_packed, 0, sizeof(lsf_packed));

  if (lich_counter == 5)
  {

    //need to pack bytes for the sw5wwp variant of the crc (might as well, may be useful in the future)
    for (i = 0; i < 30; i++)
      lsf_packed[i] = (uint8_t)ConvertBitIntoBytes(&state->m17_lsf[i*8], 8);

    crc_cmp = crc16m17(lsf_packed, 28);
    crc_ext = (uint16_t)ConvertBitIntoBytes(&state->m17_lsf[224], 16);

    if (crc_cmp != crc_ext) crc_err = 1;

    if (crc_err == 0)
      M17decodeLSF(state);

    if (opts->payload == 1)
    {
      fprintf (stderr, "\n LSF: ");
      for (i = 0; i < 30; i++)
      {
        if (i == 15) fprintf (stderr, "\n      ");
        fprintf (stderr, "[%02X]", lsf_packed[i]);
      }
    }

    memset (state->m17_lsf, 1, sizeof(state->m17_lsf));

    //debug
    // fprintf (stderr, " E-%04X; C-%04X (CRC CHK)", crc_ext, crc_cmp);

    if (crc_err == 1) fprintf (stderr, " EMB LSF CRC ERR");
  }

  return err;
}

void M17processCodec2_1600(dsd_opts * opts, dsd_state * state, uint8_t * payload)
{

  int i;
  unsigned char voice1[8];
  unsigned char voice2[8];

  for (i = 0; i < 8; i++)
  {
    voice1[i] = (unsigned char)ConvertBitIntoBytes(&payload[i*8+0], 8);
    voice2[i] = (unsigned char)ConvertBitIntoBytes(&payload[i*8+64], 8);
  }

  //TODO: Add some decryption methods
  if (state->m17_enc != 0)
  {
    //process scrambler or AES-CTR decryption 
    //(no AES-CTR right now, Scrambler should be easy enough)
  }

  if (opts->payload == 1)
  {
    fprintf (stderr, "\n CODEC2: ");
    for (i = 0; i < 8; i++)
      fprintf (stderr, "%02X", voice1[i]);
    fprintf (stderr, " (1600)");

    fprintf (stderr, "\n A_DATA: "); //arbitrary data
    for (i = 0; i < 8; i++)
      fprintf (stderr, "%02X", voice2[i]);
  }
  
  #ifdef USE_CODEC2
  size_t nsam;
  nsam = 320;

  //converted to using allocated memory pointers to prevent the overflow issues
  short * samp1 = malloc (sizeof(short) * nsam);
  short * upsamp = malloc (sizeof(short) * nsam * 6);
  short * out = malloc (sizeof(short) * 6);
  short prev;
  int j;

  codec2_decode(state->codec2_1600, samp1, voice1);

  if (opts->audio_out_type == 0 && state->m17_enc == 0) //Pulse Audio
  {
    pa_simple_write(opts->pulse_digi_dev_out, samp1, nsam*2, NULL);
  }

  if (opts->audio_out_type == 8 && state->m17_enc == 0) //UDP Audio
  {
    udp_socket_blaster (opts, state, nsam*2, samp1);
  }
    
  if (opts->audio_out_type == 5 && state->m17_enc == 0) //OSS 48k/1
  {
    //upsample to 48k and then play
    prev = samp1[0];
    for (i = 0; i < 160; i++)
    {
      upsampleS (samp1[i], prev, out);
      for (j = 0; j < 6; j++) upsamp[(i*6)+j] = out[j];
    }
    write (opts->audio_out_fd, upsamp, nsam*2*6);

  }

  if (opts->audio_out_type == 1 && state->m17_enc == 0) //STDOUT
  {
    write (opts->audio_out_fd, samp1, nsam*2);
  }

  if (opts->audio_out_type == 2 && state->m17_enc == 0) //OSS 8k/1
  {
    write (opts->audio_out_fd, samp1, nsam*2);
  }

  //WIP: Wav file saving -- still need a way to open/close/label wav files similar to call history
  if(opts->wav_out_f != NULL && state->m17_enc == 0) //WAV
  {
    sf_write_short(opts->wav_out_f, samp1, nsam);
  }

  //TODO: Codec2 Raw file saving
  // if(mbe_out_dir)
  // {

  // }

  free (samp1);
  free (upsamp);
  free (out);

  #endif

}

void M17processCodec2_3200(dsd_opts * opts, dsd_state * state, uint8_t * payload)
{
  int i;
  unsigned char voice1[8];
  unsigned char voice2[8];

  for (i = 0; i < 8; i++)
  {
    voice1[i] = (unsigned char)ConvertBitIntoBytes(&payload[i*8+0], 8);
    voice2[i] = (unsigned char)ConvertBitIntoBytes(&payload[i*8+64], 8);
  }

  //TODO: Add some decryption methods
  if (state->m17_enc != 0)
  {
    //process scrambler or AES-CTR decryption 
    //(no AES-CTR right now, Scrambler should be easy enough)
  }

  if (opts->payload == 1)
  {
    fprintf (stderr, "\n CODEC2: ");
    for (i = 0; i < 8; i++)
      fprintf (stderr, "%02X", voice1[i]);
    fprintf (stderr, " (3200)");

    fprintf (stderr, "\n CODEC2: ");
    for (i = 0; i < 8; i++)
      fprintf (stderr, "%02X", voice2[i]);
    fprintf (stderr, " (3200)");
  }
  
  #ifdef USE_CODEC2
  size_t nsam;
  nsam = 160;

  //converted to using allocated memory pointers to prevent the overflow issues
  short * samp1 = malloc (sizeof(short) * nsam);
  short * samp2 = malloc (sizeof(short) * nsam);
  short * upsamp = malloc (sizeof(short) * nsam * 6);
  short * out = malloc (sizeof(short) * 6);
  short prev;
  int j;

  codec2_decode(state->codec2_3200, samp1, voice1);
  codec2_decode(state->codec2_3200, samp2, voice2);

  if (opts->audio_out_type == 0 && state->m17_enc == 0) //Pulse Audio
  {
    pa_simple_write(opts->pulse_digi_dev_out, samp1, nsam*2, NULL);
    pa_simple_write(opts->pulse_digi_dev_out, samp2, nsam*2, NULL);
  }

  if (opts->audio_out_type == 8 && state->m17_enc == 0) //UDP Audio
  {
    udp_socket_blaster (opts, state, nsam*2, samp1);
    udp_socket_blaster (opts, state, nsam*2, samp2);
  }
    
  if (opts->audio_out_type == 5 && state->m17_enc == 0) //OSS 48k/1
  {
    //upsample to 48k and then play
    prev = samp1[0];
    for (i = 0; i < 160; i++)
    {
      upsampleS (samp1[i], prev, out);
      for (j = 0; j < 6; j++) upsamp[(i*6)+j] = out[j];
    }
    write (opts->audio_out_fd, upsamp, nsam*2*6);
    prev = samp2[0];
    for (i = 0; i < 160; i++)
    {
      upsampleS (samp2[i], prev, out);
      for (j = 0; j < 6; j++) upsamp[(i*6)+j] = out[j];
    }
    write (opts->audio_out_fd, upsamp, nsam*2*6);
  }

  if (opts->audio_out_type == 1 && state->m17_enc == 0) //STDOUT
  {
    write (opts->audio_out_fd, samp1, nsam*2);
    write (opts->audio_out_fd, samp2, nsam*2);
  }

  if (opts->audio_out_type == 2 && state->m17_enc == 0) //OSS 8k/1 
  {
    write (opts->audio_out_fd, samp1, nsam*2);
    write (opts->audio_out_fd, samp2, nsam*2);
  }

  //WIP: Wav file saving -- still need a way to open/close/label wav files similar to call history
  if(opts->wav_out_f != NULL && state->m17_enc == 0) //WAV
  {
    sf_write_short(opts->wav_out_f, samp1, nsam);
    sf_write_short(opts->wav_out_f, samp2, nsam);
  }

  //TODO: Codec2 Raw file saving
  // if(mbe_out_dir)
  // {

  // }

  free (samp1);
  free (samp2);
  free (upsamp);
  free (out);

  #endif

}

void M17prepareStream(dsd_opts * opts, dsd_state * state, uint8_t * m17_bits)
{

  int i, k, x; 
  uint8_t m17_punc[275]; //25 * 11 = 275
  memset (m17_punc, 0, sizeof(m17_punc));
  for (i = 0; i < 272; i++)
    m17_punc[i] = m17_bits[i+96];

  //depuncture the bits
  uint8_t m17_depunc[300]; //25 * 12 = 300
  memset (m17_depunc, 0, sizeof(m17_depunc));
  k = 0; x = 0;
  for (i = 0; i < 25; i++)
  {
    m17_depunc[k++] = m17_punc[x++];
    m17_depunc[k++] = m17_punc[x++];
    m17_depunc[k++] = m17_punc[x++];
    m17_depunc[k++] = m17_punc[x++];
    m17_depunc[k++] = m17_punc[x++];
    m17_depunc[k++] = m17_punc[x++];
    m17_depunc[k++] = m17_punc[x++];
    m17_depunc[k++] = m17_punc[x++];
    m17_depunc[k++] = m17_punc[x++];
    m17_depunc[k++] = m17_punc[x++];
    m17_depunc[k++] = m17_punc[x++];
    m17_depunc[k++] = 0; 
  }

  //setup the convolutional decoder
  uint8_t temp[300];
  uint8_t s0;
  uint8_t s1;
  uint8_t m_data[28];
  uint8_t trellis_buf[144];
  memset (trellis_buf, 0, sizeof(trellis_buf));
  memset (temp, 0, sizeof (temp));
  memset (m_data, 0, sizeof (m_data));

  for (i = 0; i < 296; i++)
    temp[i] = m17_depunc[i] << 1; 

  CNXDNConvolution_start();
  for (i = 0; i < 148; i++)
  {
    s0 = temp[(2*i)];
    s1 = temp[(2*i)+1];

    CNXDNConvolution_decode(s0, s1);
  }

  CNXDNConvolution_chainback(m_data, 144);

  //144/8 = 18, last 4 (144-148) are trailing zeroes
  for(i = 0; i < 18; i++)
  {
    trellis_buf[(i*8)+0] = (m_data[i] >> 7) & 1;
    trellis_buf[(i*8)+1] = (m_data[i] >> 6) & 1;
    trellis_buf[(i*8)+2] = (m_data[i] >> 5) & 1;
    trellis_buf[(i*8)+3] = (m_data[i] >> 4) & 1;
    trellis_buf[(i*8)+4] = (m_data[i] >> 3) & 1;
    trellis_buf[(i*8)+5] = (m_data[i] >> 2) & 1;
    trellis_buf[(i*8)+6] = (m_data[i] >> 1) & 1;
    trellis_buf[(i*8)+7] = (m_data[i] >> 0) & 1;
  }

  //load m_data into bits for either data packets or voice packets
  uint8_t payload[128];
  uint8_t end = 9;
  uint16_t fn = 0;
  memset (payload, 0, sizeof(payload));

  end = trellis_buf[0];
  fn = (uint16_t)ConvertBitIntoBytes(&trellis_buf[1], 15);

  //insert fn bits into meta 14 and meta 15 for Initialization Vector
  state->m17_meta[14] = (uint8_t)ConvertBitIntoBytes(&trellis_buf[1], 7);
  state->m17_meta[15] = (uint8_t)ConvertBitIntoBytes(&trellis_buf[8], 8);

  if (opts->payload == 1)
    fprintf (stderr, " FSN: %05d", fn);

  if (end == 1)
    fprintf (stderr, " END;");

  for (i = 0; i < 128; i++)
    payload[i] = trellis_buf[i+16];

  if (state->m17_str_dt == 2)
    M17processCodec2_3200(opts, state, payload);
  else if (state->m17_str_dt == 3)
    M17processCodec2_1600(opts, state, payload);
  else if (state->m17_str_dt == 1) fprintf (stderr, " DATA;");
  else if (state->m17_str_dt == 0) fprintf (stderr, "  RES;");
  // else                             fprintf (stderr, "  UNK;");

  if (opts->payload == 1 && state->m17_str_dt < 2)
  {
    fprintf (stderr, "\n STREAM: ");
    for (i = 0; i < 18; i++) 
      fprintf (stderr, "[%02X]", (uint8_t)ConvertBitIntoBytes(&trellis_buf[i*8], 8));
  }

}

void processM17STR(dsd_opts * opts, dsd_state * state)
{

  int i, x;
  //overflow/memory issue returns in Cygwin for...reasons...
  uint8_t dbuf[384]; //384-bit frame - 16-bit (8 symbol) sync pattern (184 dibits)
  uint8_t m17_rnd_bits[368]; //368 bits that are still scrambled (randomized)
  uint8_t m17_int_bits[368]; //368 bits that are still interleaved
  uint8_t m17_bits[368]; //368 bits that have been de-interleaved and de-scramble
  uint8_t lich_bits[96];
  int lich_err = -1;

  memset (dbuf, 0, sizeof(dbuf));
  memset (m17_rnd_bits, 0, sizeof(m17_rnd_bits));
  memset (m17_int_bits, 0, sizeof(m17_int_bits));
  memset (m17_bits, 0, sizeof(m17_bits));
  memset (lich_bits, 0, sizeof(lich_bits));

  //load dibits into dibit buffer
  for (i = 0; i < 184; i++)
    dbuf[i] = (uint8_t) getDibit(opts, state);

  //convert dbuf into a bit array
  for (i = 0; i < 184; i++)
  {
    m17_rnd_bits[i*2+0] = (dbuf[i] >> 1) & 1;
    m17_rnd_bits[i*2+1] = (dbuf[i] >> 0) & 1;
  }

  //descramble the frame
  for (i = 0; i < 368; i++)
    m17_int_bits[i] = (m17_rnd_bits[i] ^ m17_scramble[i]) & 1;

  //deinterleave the bit array using Quadratic Permutation Polynomial
  //function π(x) = (45x + 92x^2 ) mod 368
  for (i = 0; i < 368; i++)
  {
    x = ((45*i)+(92*i*i)) % 368;
    m17_bits[i] = m17_int_bits[x];
  }

  for (i = 0; i < 96; i++)
    lich_bits[i] = m17_bits[i];

  //check lich first, and handle LSF chunk and completed LSF
  lich_err = M17processLICH(state, opts, lich_bits);

  if (lich_err == 0)
    M17prepareStream(opts, state, m17_bits);

  // if (lich_err != 0) state->lastsynctype = -1; //disable

  //ending linebreak
  fprintf (stderr, "\n");

} //end processM17STR

void processM17LSF(dsd_opts * opts, dsd_state * state)
{

  //NOTE: Have not been able to get this to work successfully, but considering
  //there is only one LSF frame at the beginning of TX, its probably just as easy to
  //use and rely on the STR frame and embedded LSF chunk, which does work properly
  int i, j, k, x;
  uint8_t dbuf[184]; //384-bit frame - 16-bit (8 symbol) sync pattern (184 dibits)
  uint8_t m17_rnd_bits[368]; //368 bits that are still scrambled (randomized)
  uint8_t m17_int_bits[368]; //368 bits that are still interleaved
  uint8_t m17_bits[368]; //368 bits that have been de-interleaved and de-scramble
  uint8_t m17_depunc[488]; //488 bits after depuncturing

  memset (dbuf, 0, sizeof(dbuf));
  memset (m17_rnd_bits, 0, sizeof(m17_rnd_bits));
  memset (m17_int_bits, 0, sizeof(m17_int_bits));
  memset (m17_bits, 0, sizeof(m17_bits));
  memset (m17_depunc, 0, sizeof(m17_depunc));

  //load dibits into dibit buffer
  for (i = 0; i < 184; i++)
    dbuf[i] = (uint8_t) getDibit(opts, state);

  //convert dbuf into a bit array
  for (i = 0; i < 184; i++)
  {
    m17_rnd_bits[i*2+0] = (dbuf[i] >> 1) & 1;
    m17_rnd_bits[i*2+1] = (dbuf[i] >> 0) & 1;
  }

  //descramble the frame
  for (i = 0; i < 368; i++)
    m17_int_bits[i] = (m17_rnd_bits[i] ^ m17_scramble[i]) & 1;

  //deinterleave the bit array using Quadratic Permutation Polynomial
  //function π(x) = (45x + 92x^2 ) mod 368
  for (i = 0; i < 368; i++)
  {
    x = ((45*i)+(92*i*i)) % 368;
    m17_bits[i] = m17_int_bits[x];
  }

  j = 0; k = 0; x = 0;

  // P1 Depuncture
  for (i = 0; i < 488; i++)
  {
    if (p1[k++] == 1) m17_depunc[x++] = m17_bits[j++];
    else m17_depunc[x++] = 0;

    if (k == 61) k = 0; //61 -- should reset 8 times againt the array

  }

  //debug -- values seem okay at end of run
  // fprintf (stderr, "K = %d; J = %d; X = %d", k, j, x);

  //setup the convolutional decoder
  uint8_t temp[488];
  uint8_t s0;
  uint8_t s1;
  uint8_t m_data[30];
  uint8_t trellis_buf[240]; //30*8 = 240
  memset (trellis_buf, 0, sizeof(trellis_buf));
  memset (temp, 0, sizeof (temp));
  memset (m_data, 0, sizeof (m_data));

  for (i = 0; i < 488; i++)
    temp[i] = m17_depunc[i] << 1; 

  CNXDNConvolution_start();
  for (i = 0; i < 244; i++)
  {
    s0 = temp[(2*i)];
    s1 = temp[(2*i)+1];

    CNXDNConvolution_decode(s0, s1);
  }

  CNXDNConvolution_chainback(m_data, 240);

  //244/8 = 30, last 4 (244-248) are trailing zeroes
  for(i = 0; i < 30; i++)
  {
    trellis_buf[(i*8)+0] = (m_data[i] >> 7) & 1;
    trellis_buf[(i*8)+1] = (m_data[i] >> 6) & 1;
    trellis_buf[(i*8)+2] = (m_data[i] >> 5) & 1;
    trellis_buf[(i*8)+3] = (m_data[i] >> 4) & 1;
    trellis_buf[(i*8)+4] = (m_data[i] >> 3) & 1;
    trellis_buf[(i*8)+5] = (m_data[i] >> 2) & 1;
    trellis_buf[(i*8)+6] = (m_data[i] >> 1) & 1;
    trellis_buf[(i*8)+7] = (m_data[i] >> 0) & 1;
  }

  memset (state->m17_lsf, 0, sizeof(state->m17_lsf));
  memcpy (state->m17_lsf, trellis_buf, 240);

  uint8_t lsf_packed[30];
  memset (lsf_packed, 0, sizeof(lsf_packed));

  //need to pack bytes for the sw5wwp variant of the crc (might as well, may be useful in the future)
  for (i = 0; i < 30; i++)
    lsf_packed[i] = (uint8_t)ConvertBitIntoBytes(&state->m17_lsf[i*8], 8);

  uint16_t crc_cmp = crc16m17(lsf_packed, 28);
  uint16_t crc_ext = (uint16_t)ConvertBitIntoBytes(&state->m17_lsf[224], 16);
  int crc_err = 0;

  if (crc_cmp != crc_ext) crc_err = 1;

  if (crc_err == 0)
    M17decodeLSF(state);

  if (opts->payload == 1)
  {
    fprintf (stderr, "\n LSF: ");
    for (i = 0; i < 30; i++)
    {
      if (i == 15) fprintf (stderr, "\n      ");
      fprintf (stderr, "[%02X]", lsf_packed[i]);
    }
  }

  //debug
  // fprintf (stderr, " E-%04X; C-%04X (CRC CHK)", crc_ext, crc_cmp);

  if (crc_err == 1) fprintf (stderr, " CRC ERR");

  //ending linebreak
  fprintf (stderr, "\n");

} //end processM17LSF

//debug M17STR for the encoder to pass bits into
void processM17STR_debug(dsd_opts * opts, dsd_state * state, uint8_t * m17_rnd_bits)
{

  int i, x;
  uint8_t m17_int_bits[368]; //368 bits that are still interleaved
  uint8_t m17_bits[368]; //368 bits that have been de-interleaved and de-scrambled
  uint8_t lich_bits[96];
  int lich_err = -1;

  memset (m17_int_bits, 0, sizeof(m17_int_bits));
  memset (m17_bits, 0, sizeof(m17_bits));
  memset (lich_bits, 0, sizeof(lich_bits));

  //descramble the frame
  for (i = 0; i < 368; i++)
    m17_int_bits[i] = (m17_rnd_bits[i] ^ m17_scramble[i]) & 1;

  //deinterleave the bit array using Quadratic Permutation Polynomial
  //function π(x) = (45x + 92x^2 ) mod 368
  for (i = 0; i < 368; i++)
  {
    x = ((45*i)+(92*i*i)) % 368; UNUSED(x);
    m17_bits[i] = m17_int_bits[x];
  }

  for (i = 0; i < 96; i++)
    lich_bits[i] = m17_bits[i];

  //check lich first, and handle LSF chunk and completed LSF
  lich_err = M17processLICH(state, opts, lich_bits);

  if (lich_err == 0)
    M17prepareStream(opts, state, m17_bits);


} //end processM17STR_debug

//simple convolutional encoder
void simple_conv_encoder (uint8_t * input, uint8_t * output, int len)
{
  int i, k = 0;
  uint8_t g1 = 0;
  uint8_t g2 = 0;
  uint8_t d  = 0;
  uint8_t d1 = 0;
  uint8_t d2 = 0;
  uint8_t d3 = 0;
  uint8_t d4 = 0;

  for (i = 0U; i < len; i++)
  {
    d = input[i];

    g1 = (d + d3 + d4) & 1;
    g2 = (d + d1 + d2 + d4) & 1;

    d4 = d3;
    d3 = d2;
    d2 = d1;
    d1 = d;

    output[k++] = g1;
    output[k++] = g2;
  }
}

//dibits-symbols map (TX)
const int8_t symbol_map[4]={+1, +3, -1, -3};

//encode and create audio of a Project M17 STR signal
void encodeM17STR(dsd_opts * opts, dsd_state * state)
{

  //TODO: Cleanup Tons of Unused Arrays and Variables
  //TODO: Switch to M17 Viterbi Decoder?

  //User Defined Variables
  uint8_t can = 7;
  //numerical representation of dst and src after encoding, or special/reserved value
  unsigned long long int dst = 0;
  unsigned long long int src = 0;
  //DST and SRC Callsign Data (up to 9 characters in the b40 array)
  char d40[] = "ABCDEFGHI"; UNUSED(d40); //DST
  char s40[] = "IHGFEDCBA"; UNUSED(s40); //SRC
  //end User Defined Variables
  
  int i, j, k, x; //basic utility counters
  short sample = 0;
  size_t nsam = 160;  //number of samples to be read in (default is for codec2 3200 bps)
  size_t nbit = 64; //number of bits in each frame

  #ifdef USE_CODEC2
  nsam = codec2_samples_per_frame(state->codec2_3200);
  nbit = codec2_bits_per_frame(state->codec2_3200);
  #endif

  short * samp1 = malloc (sizeof(short) * nsam);
  short * samp2 = malloc (sizeof(short) * nsam);

  short voice1[nsam]; //read in xxx ms of audio from input source
  short voice2[nsam]; //read in xxx ms of audio from input source
  
  int fsn, eot = 0;  //frame sequence number and eot bit
  int lich_cnt = 0; //lich frame number counter

  uint8_t m17_lich[48]; UNUSED(m17_lich); //48-bit LICH (chunk+lich_cnt+reserved bits)
  uint8_t lsf_chunk[6][48]; UNUSED(lsf_chunk); //40 bit chunks of link information spread across 6 frames
  uint8_t lsf_chunkr[6][48]; UNUSED(lsf_chunkr); //reverse order bits
  uint8_t m17_lsf[240]; UNUSED(m17_lsf); //the complete LSF

  memset (m17_lich, 0, sizeof(m17_lich));
  memset (lsf_chunk, 0, sizeof(lsf_chunk));
  memset (lsf_chunkr, 0, sizeof(lsf_chunkr));
  memset (m17_lsf, 0, sizeof(m17_lsf));

  //NOTE: Most lich and lsf_chunk bits can be pre-set before the while loop,
  //only need to refresh the lich_cnt value and golay (no meta/IV/enc)
  uint16_t lsf_dt = 2; //voice (3200bps)
  uint16_t lsf_et = 0; UNUSED(lsf_et);
  uint16_t lsf_es = 0; UNUSED(lsf_es);
  uint16_t lsf_cn = can; //can value
  uint16_t lsf_rs = 0; UNUSED(lsf_rs);

  uint16_t lsf_fi = 0; //16-bit frame information
  lsf_fi = (lsf_dt << 1) + (lsf_cn << 7);
  for (i = 0; i < 16; i++) m17_lsf[96+i] = (lsf_fi >> 15-i) & 1;

  //TODO: Only the below if not reserved values "BROADCAST", "ALL", etc

  //Convert base40 CSD to numerical values (lifted from libM17)
  for(i = strlen((const char*)d40)-1; i >= 0; i--)
  {
    for(j = 0; j < 40; j++)
    {
      if(d40[i]==b40[j])
      {
        dst=dst*40+j;
        break;
      }
    }
  }

  for(i = strlen((const char*)s40)-1; i >= 0; i--)
  {
    for(j = 0; j < 40; j++)
    {
      if(s40[i]==b40[j])
      {
        src=src*40+j;
        break;
      }
    }
  }
  //end CSD conversion

  for (i = 0; i < 48; i++) m17_lsf[i] = (dst >> 47ULL-(unsigned long long int)i) & 1;
  for (i = 0; i < 48; i++) m17_lsf[i+48] = (src >> 47ULL-(unsigned long long int)i) & 1;

  //compute the CRC16 for LSF
  uint16_t crc_cmp = 0;
  uint8_t lsf_packed[30];
  memset (lsf_packed, 0, sizeof(lsf_packed));
  for (i = 0; i < 30; i++)
      lsf_packed[i] = (uint8_t)ConvertBitIntoBytes(&m17_lsf[i*8], 8);
  crc_cmp = crc16m17(lsf_packed, 28);

  //attach the crc16 bits to the end of the LSF data
  for (i = 0; i < 16; i++) m17_lsf[224+i] = (crc_cmp >> 15-i) & 1;


  fprintf (stderr, "M17 Voice Stream Encoding: %ld Samples and %ld Bits Per Frame (3200 bps); ", nsam, nbit);
  
  //TODO: Make this Ncurses + Transmit Button?
  while (!exitflag) //while the software is running
  {
    //read some audio samples from mic and load them into an audio buffer
    //NOTE: Reconfigured to use default 48k input, take 1 out of every 6 samples (Codec2 is 8k encoder)
    if (opts->audio_in_type == 0) //pulse audio
    {
      for (i = 0; i < nsam; i++)
      {
        for (j = 0; j < 6; j++)
          pa_simple_read(opts->pulse_digi_dev_in, &sample, 2, NULL );
        voice1[i] = sample; //only store the 6th sample
      }

      for (i = 0; i < nsam; i++)
      {
        for (j = 0; j < 6; j++)
          pa_simple_read(opts->pulse_digi_dev_in, &sample, 2, NULL );
        voice2[i] = sample; //only store the 6th sample
      }
    }

    else if (opts->audio_in_type == 1) //stdin
    {
      int result = 0;
      for (i = 0; i < nsam; i++)
      {
        for (j = 0; j < 6; j++)
          result = sf_read_short(opts->audio_in_file, &sample, 1);
        voice1[i] = sample;
        if (result == 0)
        {
          sf_close(opts->audio_in_file);
          fprintf (stderr, "Connection to STDIN Disconnected.\n");
          fprintf (stderr, "Closing DSD-FME.\n");
          exitflag = 1;
          break;
        }
      }

      for (i = 0; i < nsam; i++)
      {
        for (j = 0; j < 6; j++)
          result = sf_read_short(opts->audio_in_file, &sample, 1);
        voice2[i] = sample;
        if (result == 0)
        {
          sf_close(opts->audio_in_file);
          fprintf (stderr, "Connection to STDIN Disconnected.\n");
          fprintf (stderr, "Closing DSD-FME.\n");
          exitflag = 1;
          break;
        }
      }
    }

    else if (opts->audio_in_type == 5) //OSS, testing working
    {
      for (i = 0; i < nsam; i++)
      {
        for (j = 0; j < 6; j++)
          read (opts->audio_in_fd, &sample, 2);
        voice1[i] = sample;
      }

      for (i = 0; i < nsam; i++)
      {
        for (j = 0; j < 6; j++)
          read (opts->audio_in_fd, &sample, 2);
        voice2[i] = sample;
      }
    }

    else if (opts->audio_in_type == 8) //TCP
    {
      int result = 0;
      for (i = 0; i < nsam; i++)
      {
        for (j = 0; j < 6; j++)
          result = sf_read_short(opts->tcp_file_in, &sample, 1);
        voice1[i] = sample;
        if (result == 0)
        {
          sf_close(opts->tcp_file_in);
          fprintf (stderr, "Connection to TCP Server Disconnected.\n");
          fprintf (stderr, "Closing DSD-FME.\n");
          exitflag = 1;
          break;
        }
      }

      for (i = 0; i < nsam; i++)
      {
        for (j = 0; j < 6; j++)
          result = sf_read_short(opts->tcp_file_in, &sample, 1);
        voice2[i] = sample;
        if (result == 0)
        {
          sf_close(opts->tcp_file_in);
          fprintf (stderr, "Connection to TCP Server Disconnected.\n");
          fprintf (stderr, "Closing DSD-FME.\n");
          exitflag = 1;
          break;
        }
      }
    }

    else if (opts->audio_in_type == 3)
    {
      #ifdef USE_RTLSDR
      for (i = 0; i < nsam; i++)
      {
        for (j = 0; j < 6; j++)
          if (get_rtlsdr_sample(&sample, opts, state) < 0)
            cleanupAndExit(opts, state);
        voice1[i] = sample;
      }

      for (i = 0; i < nsam; i++)
      {
        for (j = 0; j < 6; j++)
          if (get_rtlsdr_sample(&sample, opts, state) < 0)
            cleanupAndExit(opts, state);
        voice2[i] = sample;
      }
      #endif
    }

    //convert out audio input into CODEC2 (3200bps) 8 byte data stream
    uint8_t vc1_bytes[8]; memset (vc1_bytes, 0, sizeof(vc1_bytes));
    uint8_t vc2_bytes[8]; memset (vc2_bytes, 0, sizeof(vc2_bytes));

    #ifdef USE_CODEC2
    codec2_encode(state->codec2_3200, vc1_bytes, voice1);
    codec2_encode(state->codec2_3200, vc2_bytes, voice2);
    #endif

    //start assembling our completed frame

    //STR frame sync pattern - 0xFF5D //-3, -3, -3, -3, +3, +3, -3, +3
    uint8_t m17_str_fs[16] = {1,1,1,1,1,1,1,1,0,1,0,1,1,1,0,1}; UNUSED(m17_str_fs);

    //Data/Voice Portion of Stream Data Link Layer w/ FSN
    uint8_t m17_v1[148]; memset (m17_v1, 0, sizeof(m17_v1)); UNUSED(m17_v1);

    //Data/Voice Portion of Stream Data Link Layer w/ FSN (after Convolutional Encode)
    uint8_t m17_v1c[296]; memset (m17_v1c, 0, sizeof(m17_v1c)); UNUSED(m17_v1c);

    //Data/Voice Portion of Stream Data Link Layer w/ FSN (after P2 Puncturing)
    uint8_t m17_v1p[272]; memset (m17_v1p, 0, sizeof(m17_v1p)); UNUSED(m17_v1p);

    //LSF Chunk + LICH CNT of Stream Data Link Layer
    uint8_t m17_l1[48]; memset (m17_l1, 0, sizeof(m17_l1)); UNUSED(m17_l1);

    //LSF Chunk + LICH CNT of Stream Data Link Layer (after Golay 24,12 Encoding)
    uint8_t m17_l1g[96]; memset (m17_l1g, 0, sizeof(m17_l1g)); UNUSED(m17_l1g);

    //Type 4c - Combined LSF Content Chuck and Voice/Data (96 + 272)
    uint8_t m17_t4c[368]; memset (m17_t4c, 0, sizeof(m17_t4c)); UNUSED(m17_t4c);

    //Type 4i - Interleaved Bits
    uint8_t m17_t4i[368]; memset (m17_t4i, 0, sizeof(m17_t4i)); UNUSED(m17_t4i);

    //Type 4s - Interleaved Bits with Scrambling Applied
    uint8_t m17_t4s[368]; memset (m17_t4s, 0, sizeof(m17_t4s)); UNUSED(m17_t4s);

    //OTA Symbol Convertion (best variable type for this? int16?)
    uint8_t m17_sym[184]; memset (m17_sym, 0, sizeof(m17_sym)); UNUSED(m17_sym);

    //insert the voice bytes into voice bits, and voice bits into v1 in their appropriate location
    uint8_t v1_bits[64]; memset (v1_bits, 0, sizeof(v1_bits)); UNUSED(v1_bits);
    uint8_t v2_bits[64]; memset (v2_bits, 0, sizeof(v2_bits)); UNUSED(v2_bits);

    k = 0; x = 0;
    for (j = 0; j < 8; j++)
    {
      for (i = 0; i < 8; i++)
      {
        v1_bits[k++] = (vc1_bytes[j] >> 7-i) & 1;
        v2_bits[x++] = (vc2_bytes[j] >> 7-i) & 1;
      }
    }

    for (i = 0; i < 64; i++)
    {
      m17_v1[i+16]    = v1_bits[i];
      m17_v1[i+16+64] = v2_bits[i];
    }

    //set end of tx bit on the exitflag (sig)
    if (exitflag) eot = 1;
    m17_v1[0] = (uint8_t)eot; //set as first bit of the stream
    //set current frame number as bits 1-15 of the v1 stream
    for (i = 0; i < 15; i++)
      m17_v1[i+1] = ( (uint8_t)(fsn >> 14-i) ) &1;
    fsn++; //increment the frame sequence number

    //Use the convolutional encoder to encode the voice / data stream
    simple_conv_encoder (m17_v1, m17_v1c, 144); //seems to be working now, or at least, the same bits before and after on the playback side

    //use the P2 puncture to...puncture and collapse the voice / data stream
    k = 0; x = 0;
    for (i = 0; i < 25; i++)
    {
      m17_v1p[k++] = m17_v1c[x++];
      m17_v1p[k++] = m17_v1c[x++];
      m17_v1p[k++] = m17_v1c[x++];
      m17_v1p[k++] = m17_v1c[x++];
      m17_v1p[k++] = m17_v1c[x++];
      m17_v1p[k++] = m17_v1c[x++];
      if (k == 272) break; //quit early on last set of i when 272 k bits reached
      m17_v1p[k++] = m17_v1c[x++];
      if (k == 272) break; //quit early on last set of i when 272 k bits reached
      m17_v1p[k++] = m17_v1c[x++];
      if (k == 272) break; //quit early on last set of i when 272 k bits reached
      m17_v1p[k++] = m17_v1c[x++];
      if (k == 272) break; //quit early on last set of i when 272 k bits reached
      m17_v1p[k++] = m17_v1c[x++];
      m17_v1p[k++] = m17_v1c[x++];
      x++;
    }

    //add punctured voice / data bits to the combined frame
    for (i = 0; i < 272; i++)
      m17_t4c[i+96] = m17_v1p[i];

    //ignore the LSF/LICH reserved bits (unused currently)

    //load up the lsf chunk for this cnt
    for (i = 0; i < 40; i++)
      lsf_chunk[lich_cnt][i] = m17_lsf[((lich_cnt)*40)+i];

    //update lich_cnt in the current LSF chunk
    lsf_chunk[lich_cnt][40] = (lich_cnt >> 2) & 1;
    lsf_chunk[lich_cnt][41] = (lich_cnt >> 1) & 1;
    lsf_chunk[lich_cnt][42] = (lich_cnt >> 0) & 1;

    //This is not M17 standard (yet), but use the LICH reserved bits to signal dt
    lsf_chunk[lich_cnt][46] = (lsf_dt >> 1) & 1;
    lsf_chunk[lich_cnt][47] = (lsf_dt >> 0) & 1;
    //use the last 3 bits of reserved to signal the can value
    lsf_chunk[lich_cnt][43] = (lsf_cn >> 2) & 1;
    lsf_chunk[lich_cnt][44] = (lsf_cn >> 1) & 1;
    lsf_chunk[lich_cnt][45] = (lsf_cn >> 0) & 1;

    //encode with golay 24,12 and load into m17_lig
    Golay_24_12_encode (lsf_chunk[lich_cnt]+00, m17_l1g+00);
    Golay_24_12_encode (lsf_chunk[lich_cnt]+12, m17_l1g+24);
    Golay_24_12_encode (lsf_chunk[lich_cnt]+24, m17_l1g+48);
    Golay_24_12_encode (lsf_chunk[lich_cnt]+36, m17_l1g+72);

    //add lsf chunk to the combined frame
    for (i = 0; i < 96; i++)
      m17_t4c[i] = m17_l1g[i];

    //interleave the bit array using Quadratic Permutation Polynomial
    //function π(x) = (45x + 92x^2 ) mod 368
    for (i = 0; i < 368; i++)
    {
      x = ((45*i)+(92*i*i)) % 368;
      m17_t4i[x] = m17_t4c[i];
    }

    //scramble the frame
    for (i = 0; i < 368; i++)
      m17_t4s[i] = (m17_t4i[i] ^ m17_scramble[i]) & 1;

    //-----------------------------------------

    //decode stream with the M17STR_debug
    fprintf (stderr, "\n M17 STR (ENCODER): ");
    processM17STR_debug(opts, state, m17_t4s);

    //load bits inti a dibit array plus the framesync bits
    uint8_t output_dibits[192]; memset (output_dibits, 0, sizeof(output_dibits)); UNUSED(output_dibits);
    for (i = 0; i < 8; i++)
      output_dibits[i] = (m17_str_fs[i*2+0] << 1) + (m17_str_fs[i*2+1] << 0);

    //load rest of combined frame
    for (i = 0; i < 184; i++)
      output_dibits[i+8] = (m17_t4s[i*2+0] << 1) + (m17_t4s[i*2+1] << 0);

    //Start working on converting the bitsream to an audio stream

    //convert to symbols @ 4800 bps
    int output_symbols[192]; memset (output_symbols, 0, sizeof(output_symbols)); UNUSED(output_symbols);
    for (i = 0; i < 192; i++)
      output_symbols[i] = symbol_map[output_dibits[i]];

    //debug output symbols
    // fprintf (stderr, "\n sym:");
    // for (i = 0; i < 192; i++)
    // {
    //   if (i%24 == 0) fprintf (stderr, "\n");
    //   fprintf (stderr, " %d", output_symbols[i]);
    // }

    //TODO: symbols to audio

    //end by incrementing lich_cnt, reset on 6
    lich_cnt++;
    if (lich_cnt == 6) lich_cnt = 0;
    
  }
  
  //free allocated memory
  free(samp1);
  free(samp2);

}
