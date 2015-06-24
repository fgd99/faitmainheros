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
struct debug_read_file_result
{
  uint32 ContentsSize;
  void *Contents;
};

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(void *Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_plateform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(char *Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATEFORM_WRITE_ENTIRE_FILE(name) bool32 name(char *Filename, uint32 MemorySize, void *Memory)
typedef DEBUG_PLATEFORM_WRITE_ENTIRE_FILE(debug_plateform_write_entire_file);

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
  bool32 IsConnected;
  bool32 IsAnalog;
  real32 StickAverageX;
  real32 StickAverageY;

  union
  {
    game_button_state Buttons[16];
    struct
    {
      game_button_state MoveUp;
      game_button_state MoveDown;
      game_button_state MoveLeft;
      game_button_state MoveRight;

      game_button_state ActionUp;
      game_button_state ActionDown;
      game_button_state ActionLeft;
      game_button_state ActionRight;

      game_button_state A;
      game_button_state B;
      game_button_state X;
      game_button_state Y;

      game_button_state LeftShoulder;
      game_button_state RightShoulder;

      game_button_state Back;
      game_button_state Start;
    };
  };
};

struct game_input
{
  game_controller_input Controllers[5];
};
inline game_controller_input *GetController(game_input *Input, int unsigned ControllerIndex)
{
  Assert(ControllerIndex < ArrayCount(Input->Controllers));
  game_controller_input *Result = &Input->Controllers[ControllerIndex];
  return Result;
}

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

  debug_plateform_free_file_memory *DEBUGPlatformFreeFileMemory;
  debug_platform_read_entire_file *DEBUGPlatformReadEntireFile;
  debug_plateform_write_entire_file *DEBUGPlatformWriteEntireFile;
};

// Pour permettre de charger dynamiquement la DLL du moteur de jeu
#define GAME_UPDATE_AND_RENDER(name) void name(game_memory *Memory, game_input *Input, game_offscreen_buffer *Buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);
GAME_UPDATE_AND_RENDER(GameUpdateAndRenderStub)
{
}

#define GAME_GET_SOUND_SAMPLES(name) void name(game_memory *Memory, game_sound_output_buffer *SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);
GAME_GET_SOUND_SAMPLES(GameGetSoundSamplesStub)
{
}

#define FAITMAIN_H
#endif