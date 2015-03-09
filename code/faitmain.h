#if !defined(FAITMAIN_H)

/*
  Services fournis par la couche plateforme au jeu
*/

/*
  Services fournis par le jeu à couche plateforme
*/

// Struct qui représente un backbuffer qui nous permet de dessiner
struct game_offscreen_buffer {
  // BITMAPINFO Info;
  void *Memory;
  int Width;
  int Height;
  int BytesPerPixel;
  int Pitch; // Pitch représente la taille d'une ligne en octets
};

struct game_sound_output_buffer
{
  int SamplesPerSecond;
  int SampleCount;
  int16 *Samples;
};

// Cette fonction va avoir besoin des informations de timing, du controlleur, le bitmap buffer et le sound buffer
internal void GameUpdateAndRender(game_offscreen_buffer *Buffer, int XOffset, int YOffset,
                                  game_sound_output_buffer *SoundBuffer, int ToneHz);

#define FAITMAIN_H
#endif