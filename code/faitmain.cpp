#include "faitmain.h"

void
GameOutputSound(game_sound_output_buffer *SoundBuffer, int ToneHz)
{
  local_persist real32 tSine;
  int16 ToneVolume = 3000;
  int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

  int16 *SampleOut = SoundBuffer->Samples;
  for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
  {
    real32 SineValue = sinf(tSine);
    int16 SampleValue = (int16)(SineValue * ToneVolume);
    *SampleOut++ = SampleValue;
    *SampleOut++ = SampleValue;
    tSine += (real32)(2.0f * PI32 * 1.0f / (real32)WavePeriod);
    // A partir d'un moment sinf perd sa pr�cision quand les chiffres sont tr�s hauts
    if(tSine > 2.0f*PI32)
    {
      tSine -= 2.0f*PI32;
    }
  }
}

/* Fonction qui va dessiner dans le backbuffer un gradient de couleur �trange */
void
RenderWeirdGradient(game_offscreen_buffer *Buffer, int XOffset, int YOffset)
{
  // on va se d�placer dans la m�moire par pas de 8 bits
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
      Pixels en m�moire : 00 00 00 00 ...
      Couleur             BB GG RR XX
      en hexa: 0xXXRRGGBB
      */
      uint8 Blue = (uint8)(X + XOffset);
      uint8 Green = (uint8)(Y + YOffset);
      uint8 Red = (uint8)(X + Y);
      // On peut changer directement la couleur d'un pixel :  *Pixel = 0xFF00FF00;
      // ce qui �quivaut en hexa � 0x00BBGG00
      *Pixel++ = ((Red << 16) | (Green << 8) | Blue);
    }
    Row += Buffer->Pitch; // Ligne suivante
  }
}

GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
  // On v�rifie que l'on a allou� assez de m�moire pour le jeu
  Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
  // On v�rifie que la structure des boutons des controllers est bien d�finie
  Assert(
    (&Input->Controllers[0].Start - &Input->Controllers[0].Buttons[0])
    == ArrayCount(Input->Controllers[0].Buttons) - 1);

  game_state *GameState = (game_state*)Memory->PermanentStorage;
  if (!Memory->IsInitialized)
  {
    char *Filename = __FILE__; // Le nom du fichier source en cours

    debug_read_file_result File = Memory->DEBUGPlatformReadEntireFile(Filename);
    if (File.Contents)
    {
      Memory->DEBUGPlatformWriteEntireFile("test.out", File.ContentsSize, File.Contents);
      Memory->DEBUGPlatformFreeFileMemory(File.Contents);
    }

    GameState->ToneHz = 512;
    GameState->BlueOffset = 0;
    GameState->GreenOffset = 0;
    Memory->IsInitialized = true;
  }

  for (int ControllerIndex = 0;
       ControllerIndex < ArrayCount(Input->Controllers);
       ++ControllerIndex)
  {
    game_controller_input *Controller = GetController(Input, ControllerIndex);
    // Gestion des entr�es
    if (Controller->IsAnalog)
    {
      GameState->BlueOffset += (int)(4.0f*Controller->StickAverageX);
      GameState->ToneHz = 512 + (int)(128.0f * Controller->StickAverageY);
    }
    else
    {
      if (Controller->MoveUp.EndedDown) GameState->GreenOffset += 10;
      if (Controller->MoveDown.EndedDown) GameState->GreenOffset -= 10;
      if (Controller->MoveRight.EndedDown) {
        GameState->ToneHz += 10;
        GameState->BlueOffset += 10;
      }
      if (Controller->MoveLeft.EndedDown) {
        GameState->ToneHz -= 10;
        GameState->BlueOffset -= 10;
      }
    }
  }

  
  RenderWeirdGradient(Buffer, GameState->BlueOffset, GameState->GreenOffset);
}

GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
  game_state *GameState = (game_state*)Memory->PermanentStorage;
  GameOutputSound(SoundBuffer, GameState->ToneHz);
}