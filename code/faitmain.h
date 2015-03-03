#if !defined(FAITMAIN_H)

/*
  Services fournis par la couche plateforme au jeu
*/

/*
  Services fournis par le jeu � couche plateforme
*/

// Struct qui repr�sente un backbuffer qui nous permet de dessiner
struct game_offscreen_buffer {
  // BITMAPINFO Info;
  void *Memory;
  int Width;
  int Height;
  int BytesPerPixel;
  int Pitch; // Pitch repr�sente la taille d'une ligne en octets
};

// Cette fonction va avoir besoin des informations de timing, du controlleur, le bitmap buffer et le sound buffer
internal void GameUpdateAndRender(game_offscreen_buffer *Buffer, int XOffset, int YOffset);

#define FAITMAIN_H
#endif