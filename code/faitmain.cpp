#include "faitmain.h"

/* Fonction qui va dessiner dans le backbuffer un gradient de couleur étrange */
internal void
RenderWeirdGradient(game_offscreen_buffer *Buffer, int XOffset, int YOffset)
{
  // on va se déplacer dans la mémoire par pas de 8 bits
  uint8 *Row = (uint8 *)Buffer->Memory;
  for (int Y = 0; Y < Buffer->Height; ++Y)
  {
    // Pixel par pixel, on commence par le premier de la ligne
    uint32 *Pixel = (uint32 *)Row;
    for (int X = 0; X < Buffer->Width; ++X)
    {
      /*
      Pixels en little endian architecture
      0  1  2  3 ...
      Pixels en mémoire : 00 00 00 00 ...
      Couleur             BB GG RR XX
      en hexa: 0xXXRRGGBB
      */
      uint8 Blue = (X + XOffset);
      uint8 Green = (Y + YOffset);
      uint8 Red = (X + Y);
      // On peut changer directement la couleur d'un pixel :  *Pixel = 0xFF00FF00;
      // ce qui équivaut en hexa à 0x00BBGG00
      *Pixel++ = ((Red << 16) | (Green << 8) | Blue);
    }
    Row += Buffer->Pitch; // Ligne suivante
  }
}

internal void
GameUpdateAndRender(game_offscreen_buffer *Buffer, int XOffset, int YOffset)
{
  RenderWeirdGradient(Buffer, XOffset, YOffset);
}