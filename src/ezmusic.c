#include <windows.h>
#include <dsound.h>

#define EZ_IMPLEMENTATION
#define EZ_NO_CRT_LIB
#include "ez.h"

typedef struct
wave_header
{
    char     chunk_id[4];
    uint32_t chunk_size;
    char     format[4];
    char     subchunk_id[4];
    uint32_t subchunk_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t bytes_per_second;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char     data_chunk_id[4];
    uint32_t data_size;
} wave_header;

#define DIRECT_SOUND_CREATE(name) HRESULT name(LPCGUID, LPDIRECTSOUND*, LPUNKNOWN)
typedef DIRECT_SOUND_CREATE(dsound_create);
dsound_create *Win32DirectSoundCreate;

char **
parse_args(int *argc)
{
    char **argv;

    char *cmd_line;
    int argv_cap;

    char *start;
    int len;

    char *ptr;

    cmd_line = GetCommandLineA();

    argv_cap = 4;
    argv = ez_mem_alloc(sizeof(char *)*argv_cap);

    *argc = 0;
    while(*cmd_line)
    {
        switch(*cmd_line)
        {
            case '"':
            {
                ++cmd_line;
                start = cmd_line;
                len = 0;
                while(*cmd_line && *cmd_line != '"')
                {
                    ++cmd_line;
                    ++len;
                }
                ptr = (char *)ez_mem_alloc(len + 1);
                ez_str_copy_max(start, ptr, len);
                ptr[len] = 0;

                if(*argc >= argv_cap)
                {
                    argv_cap *= 2;
                    argv = ez_mem_realloc(argv, sizeof(char *)*argv_cap);
                }
                argv[*argc] = ptr;
                *argc += 1;
            } break;

            case  ' ': case '\t': case '\v':
            case '\n': case '\r': case '\f':
            {
                ++cmd_line;
            } break;

            default:
            {
                start = cmd_line;
                len = 0;
                while(*cmd_line &&
                      *cmd_line !=  ' ' && *cmd_line != '\t' &&
                      *cmd_line != '\v' && *cmd_line != '\n' &&
                      *cmd_line != '\r' && *cmd_line != '\f')
                {
                    ++cmd_line;
                    ++len;
                }
                ptr = (char *)ez_mem_alloc(len + 1);
                ez_str_copy_max(start, ptr, len);
                ptr[len] = 0;

                if(*argc >= argv_cap)
                {
                    argv_cap *= 2;
                    argv = ez_mem_realloc(argv, sizeof(char *)*argv_cap);
                }
                argv[*argc] = ptr;
                *argc += 1;
            } break;
        }
    }

    return(argv);
}

void
main(void)
{
    int argc;
    char **argv;
    argv = parse_args(&argc);

    char *file_name = 0;

    if(argc <= 1)
    {
        char *usage = "./ezplay file";
        ez_out_println(usage);
    }
    else
    {
        file_name = argv[1];
    }

    /*
     * NOTE(driverfury): Load WAV file and parse it
     */
    size_t file_size = 0;
    void *file_content = 0;
    file_content = ez_file_read_bin(file_name, &file_size);
    if(!file_content || file_size == 0)
    {
        DWORD error = GetLastError();
        ExitProcess(error);
    }

    wave_header *wvheader = (wave_header *)file_content;
    if( (wvheader->chunk_id[0] != 'R') || (wvheader->chunk_id[1] != 'I') ||
        (wvheader->chunk_id[2] != 'F') || (wvheader->chunk_id[3] != 'F'))
    {
        ExitProcess(6);
    }
    if( (wvheader->format[0] != 'W') || (wvheader->format[1] != 'A') ||
        (wvheader->format[2] != 'V') || (wvheader->format[3] != 'E'))
    {
        ExitProcess(7);
    }
    if( (wvheader->subchunk_id[0] != 'f') || (wvheader->subchunk_id[1] != 'm') ||
        (wvheader->subchunk_id[2] != 't') || (wvheader->subchunk_id[3] != ' '))
    {
        ExitProcess(8);
    }
    if(wvheader->audio_format != WAVE_FORMAT_PCM)
    {
        ExitProcess(9);
    }
    if(wvheader->num_channels != 2)
    {
        ExitProcess(10);
    }
#if 0
    if(wvheader->sample_rate != 44100)
    {
        ExitProcess(11);
    }
    if(wvheader->bits_per_sample != 16)
    {
        ExitProcess(12);
    }
#endif

    if( (wvheader->data_chunk_id[0] != 'd') || (wvheader->data_chunk_id[1] != 'a') ||
        (wvheader->data_chunk_id[2] != 't') || (wvheader->data_chunk_id[3] != 'a'))
    {
        ExitProcess(13);
    }

    /*
     * NOTE(driverfury): Load DirectSound library
     **/
#if 0
    HWND window = GetDesktopWindow();
#else
    HWND window = GetConsoleWindow();
#endif
    HMODULE dsound_lib = LoadLibraryA("dsound.dll");

    if(!dsound_lib)
    {
        ExitProcess(1);
    }
    Win32DirectSoundCreate = (dsound_create *)
        GetProcAddress(dsound_lib, "DirectSoundCreate");
    LPDIRECTSOUND dsound;
    if( !Win32DirectSoundCreate ||
        FAILED(Win32DirectSoundCreate(0, &dsound, 0)))
    {
        ExitProcess(2);
    }
    if(FAILED(dsound->lpVtbl->SetCooperativeLevel(dsound, window, DSSCL_PRIORITY)))
    {
        ExitProcess(3);
    }
    DSBUFFERDESC buffdesc = {0};
    buffdesc.dwSize  = sizeof(buffdesc);
    buffdesc.dwFlags = DSBCAPS_PRIMARYBUFFER|DSBCAPS_CTRLVOLUME;
    buffdesc.dwBufferBytes   = 0;
    LPDIRECTSOUNDBUFFER primarybuff = 0;
    if(FAILED(dsound->lpVtbl->CreateSoundBuffer(dsound, &buffdesc, &primarybuff, 0)))
    {
        ExitProcess(4);
    }
    WAVEFORMATEX wvformat = {0};
    wvformat.wFormatTag = WAVE_FORMAT_PCM;
    wvformat.nSamplesPerSec = wvheader->sample_rate;
    wvformat.wBitsPerSample = wvheader->bits_per_sample;
    wvformat.nChannels = wvheader->num_channels;
    wvformat.nBlockAlign = (wvformat.wBitsPerSample*wvformat.nChannels) / 8;
    wvformat.nAvgBytesPerSec = wvformat.nSamplesPerSec*wvformat.nBlockAlign;
    wvformat.cbSize = 0;
    if(FAILED(primarybuff->lpVtbl->SetFormat(primarybuff, &wvformat)))
    {
        ExitProcess(5);
    }
    wvformat.wFormatTag = WAVE_FORMAT_PCM;
    wvformat.nSamplesPerSec = wvheader->sample_rate;
    wvformat.wBitsPerSample = wvheader->bits_per_sample;
    wvformat.nChannels = wvheader->num_channels;
    wvformat.nBlockAlign = (wvformat.wBitsPerSample*wvformat.nChannels) / 8;
    wvformat.nAvgBytesPerSec = wvformat.nSamplesPerSec*wvformat.nBlockAlign;
    wvformat.cbSize = 0;

    DSBUFFERDESC buffdesc2 = {0};
    buffdesc2.dwSize  = sizeof(buffdesc2);
    buffdesc2.dwFlags = DSBCAPS_GLOBALFOCUS;
    buffdesc2.dwBufferBytes = wvheader->data_size;
    buffdesc2.lpwfxFormat = &wvformat;

    LPDIRECTSOUNDBUFFER tmpbuff;
    if(FAILED(dsound->lpVtbl->CreateSoundBuffer(dsound, &buffdesc2, &tmpbuff, 0)))
    {
        ExitProcess(14);
    }
#if 0
    IDirectSoundBuffer8 *secondarybuff;
    if(FAILED(tmpbuff->lpVtbl->QueryInterface(tmpbuff, IID_IDirectSoundBuffer8, (void**)&secondarybuff)))
    {
        ExitProcess(33);
    }
#endif

    //Create a temporary buffer to hold the wave file data.
    unsigned char *wvdata = (unsigned char *)(wvheader + 1);

    // Lock the secondary buffer to write wave data into it.
    unsigned char *buffptr = 0;
    DWORD buffsize = 0;
    unsigned char *buffptr2 = 0;
    DWORD buffsize2 = 0;
    if(FAILED(tmpbuff->lpVtbl->Lock(
        tmpbuff, 0, wvheader->data_size,
        &buffptr, &buffsize,
        &buffptr2, &buffsize2, 0)))
    {
        ExitProcess(15);
    }

    // Copy the wave data into the buffer.
    ez_mem_copy(wvdata, buffptr, wvheader->data_size);

    // Unlock the secondary buffer after the data has been written to it.
    if(FAILED(tmpbuff->lpVtbl->Unlock(
        tmpbuff,
        buffptr, buffsize,
        buffptr2, buffsize2)))
    {
        ExitProcess(16);
    }

    DWORD durations;
    durations = wvheader->data_size/wvformat.nAvgBytesPerSec + 1;

    // Free file
    ez_mem_free(file_content);

    // Play sound
    if(FAILED(tmpbuff->lpVtbl->SetCurrentPosition(tmpbuff, 0)))
    {
        ExitProcess(17);
    }

    /*
    if(FAILED(tmpbuff->lpVtbl->SetVolume(tmpbuff, DSBVOLUME_MAX)))
    {
        ExitProcess(18);
    }
    */

    if(FAILED(tmpbuff->lpVtbl->Play(tmpbuff, 0, 0, 0)))
    {
        ExitProcess(19);
    }

    Sleep(durations*1000);

    /*
     * NOTE(driverfur): Release DirectSound library
     */
    primarybuff->lpVtbl->Release(primarybuff);
    dsound->lpVtbl->Release(dsound);

    ExitProcess(88);
}
