#if !defined(WIN32_HANDMADE_H)

// Struct qui repr�sente un backbuffer qui nous permet de dessiner
struct win32_offscreen_buffer {
  BITMAPINFO Info;
  void *Memory;
  int Width;
  int Height;
  int BytesPerPixel;
  int Pitch; // Pitch repr�sente la taille d'une ligne en octets
};

// Struct qui repr�sente des dimensions
struct win32_window_dimension
{
  int Width;
  int Height;
};

// Struct qui repr�sente un buffer pour jouer du son
struct win32_sound_output
{
  int SamplesPerSecond;
  int16 ToneVolume;
  uint32 RunningSampleIndex;
  int BytesPerSample;
  DWORD SecondaryBufferSize;
  int LatencySampleCount;
  real32 tSine;
};

#define WIN32_HANDMADE_H
#endif
