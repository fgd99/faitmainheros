#if !defined(FAITMAIN_H)

/*
  Macros utiles
*/
#if FAITMAIN_LENT
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif
#define Kilobytes(value) ((value)*1024LL)
#define Megabytes(value) (Kilobytes(value)*1024LL)
#define Gigabytes(value) (Megabytes(value)*1024LL)
#define Terabytes(value) (Gigabytes(value)*1024LL)
#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

/*
  Petites fonctions utiles non dépendantes de la plateforme
*/
inline uint32
SafeTruncateUint64(uint64 Value)
{
  Assert(Value <= 0xFFFFFFFF);
  return (uint32)Value;
}

/*
  Services fournis par la couche plateforme au jeu
*/
#if FAITMAIN_INTERNAL
internal void *DEBUGPlatformReadEntireFile(char *Filename);
internal void DEBUGPlatformFreeFileMemory(void *Memory);
internal bool32 DEBUGPlatformWriteEntireFile(char *Filename, uint32 MemorySize, void *Memory);
#endif

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


struct game_button_state
{
  int HalfTransitionCount;
  bool32 EndedDown;
};

struct game_controller_input
{
  bool32 IsAnalog;

  real32 StartX;
  real32 StartY;

  real32 MinX;
  real32 MinY;

  real32 MaxX;
  real32 MaxY;

  real32 EndX;
  real32 EndY;

  union
  {
    game_button_state Buttons[6];
    struct
    {
      game_button_state Up;
      game_button_state Down;
      game_button_state Left;
      game_button_state Right;

      game_button_state A;
      game_button_state B;
      game_button_state X;
      game_button_state Y;

      game_button_state LeftShoulder;
      game_button_state RightShoulder;
    };
  };
};

struct game_input
{
  game_controller_input Controllers[4];
};

struct game_state
{
  int ToneHz;
  int BlueOffset;
  int GreenOffset;
};

struct game_memory
{
  bool32 IsInitialized;
  uint64 PermanentStorageSize;
  void *PermanentStorage;
  uint64 TransientStorageSize;
  void *TransientStorage;
};

// Cette fonction va avoir besoin des informations de timing, du controlleur, le bitmap buffer et le sound buffer
internal void GameUpdateAndRender(game_memory *Memory,
                                  game_input *Input,
                                  game_offscreen_buffer *Buffer,
                                  game_sound_output_buffer *SoundBuffer);

#define FAITMAIN_H
#endif