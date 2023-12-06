/******************************************/
/*
  duplex.cpp
  by Gary P. Scavone, 2006-2019.

  This program opens a duplex stream and passes
  input directly through to the output.
*/
/******************************************/

#include "RtAudio.h"
#include <iostream>
#include <cstdlib>
#include <cstring>

#include <chrono>

class Timer
{
private:
	// Type aliases to make accessing nested type easier
	using Clock = std::chrono::steady_clock;
	using Second = std::chrono::duration<double, std::ratio<1> >;

	std::chrono::time_point<Clock> m_beg { Clock::now() };

public:
	void reset()
	{
		m_beg = Clock::now();
	}

	double elapsed() const
	{
		return std::chrono::duration_cast<Second>(Clock::now() - m_beg).count();
	}
};




/*
typedef char MY_TYPE;
#define FORMAT RTAUDIO_SINT8
*/

typedef double MY_TYPE;
#define FORMAT RTAUDIO_FLOAT64

/*
typedef S24 MY_TYPE;
#define FORMAT RTAUDIO_SINT24

typedef signed long MY_TYPE;
#define FORMAT RTAUDIO_SINT32

typedef float MY_TYPE;
#define FORMAT RTAUDIO_FLOAT32
typedef signed short MY_TYPE;
#define FORMAT RTAUDIO_SINT16
*/

constexpr unsigned int len_impulse_response{50000};

struct CallbackData{
  unsigned int bufferBytes;
  unsigned int sampleRates;

  // Input buffer data
  MY_TYPE* input_buffer_dump;
  unsigned int size_input_buffer_dump;
  int ind_input_buff_dump; 

  // Output buffer data
  MY_TYPE* output_buffer_dump;
  unsigned int size_output_buffer_dump;
  int ind_output_buff_dump;

  // Impulse response
  MY_TYPE* impulse_response;
  unsigned int len_impulse_response;

  // Convolution
  MY_TYPE* convol;
  unsigned int convol_size;
};

void usage( void );
unsigned int getDeviceIndex( std::vector<std::string> deviceNames, bool isInput = false );


int write_buff_dump(double *buff, const int n_buff, double *buff_dump, const int n_buff_dump, int *ind_dump);
int buffer_dump_to_binary_file(const char *filename, CallbackData& callbackData);
double * load_impulse_response(const char* filepath/*, unsigned int* len_impulse_response*/);





double streamTimePrintIncrement = 1.0; // seconds
double streamTimePrintTime = 1.0; // seconds

int inout( void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
           double streamTime, RtAudioStreamStatus status, void *data )
{
  MY_TYPE *inputBuff = static_cast<MY_TYPE *>(inputBuffer);
  MY_TYPE *outputBuff = static_cast<MY_TYPE *>(outputBuffer);

  // Since the number of input and output channels is equal, we can do
  // a simple buffer copy operation here.
  if ( status ) std::cout << "Stream over/underflow detected." << std::endl;

  if ( streamTime >= streamTimePrintTime ) {
    std::cout << "streamTime = " << streamTime << std::endl;
    streamTimePrintTime += streamTimePrintIncrement;
  }

  
  CallbackData *callbackData = static_cast<CallbackData*>(data);
  // Convolution temporelle (max à 2000 pour 44100)
  Timer t;
  for (unsigned int n{0}; n<callbackData->convol_size; n++) {
    unsigned int a{ callbackData->len_impulse_response - 1 };

    // callbackData->convol[n] = ( n < a+1 ? callbackData->convol[n + nBufferFrames] : 0);
    callbackData->convol[n] = 0;
    unsigned int min{ (n < a) ? 0 : n - (a - 1) };
    unsigned int max{ (n < ((nBufferFrames) - 1)) ? n : n - ((nBufferFrames) - 1) };

    for (unsigned int k{min}; k<max; k++) {
      callbackData->convol[k] += inputBuff[k] * callbackData->impulse_response[n - k];
    }
  }
  std::cout << "Time taken: " << t.elapsed() << " seconds\n";



  // Fonction de base, copie l'entrée sur la sortie
  memcpy( outputBuffer, inputBuffer, callbackData->bufferBytes );

  
  // TODO le son enregistré n'est pas de bonne qualité. On dirait qu'il maanque de l'info
  int a{ write_buff_dump(inputBuff, nBufferFrames, callbackData->input_buffer_dump, callbackData->size_input_buffer_dump, &(callbackData->ind_input_buff_dump))};
  int b{ write_buff_dump(callbackData->convol, nBufferFrames, callbackData->output_buffer_dump, callbackData->size_output_buffer_dump, &(callbackData->ind_output_buff_dump))};
   a=b;
   b=a;

  return 0;
}


constexpr unsigned int SECONDES{ 10 };


int main( int argc, char *argv[] )
{
  unsigned int channels, fs, bufferBytes, oDevice = 0, iDevice = 0, iOffset = 0, oOffset = 0;
  
  // get_process_time_windows();
  // Minimal command-line checking
  if (argc < 3 || argc > 7 ) usage();

  RtAudio adac;
  std::vector<unsigned int> deviceIds = adac.getDeviceIds();
  if ( deviceIds.size() < 1 ) {
    std::cout << "\nNo audio devices found!\n";
    exit( 1 );
  }

  channels = (unsigned int) atoi(argv[1]);
  fs = (unsigned int) atoi(argv[2]);
  if ( argc > 3 )
    iDevice = (unsigned int) atoi(argv[3]);
  if ( argc > 4 )
    oDevice = (unsigned int) atoi(argv[4]);
  if ( argc > 5 )
    iOffset = (unsigned int) atoi(argv[5]);
  if ( argc > 6 )
    oOffset = (unsigned int) atoi(argv[6]);

  // Let RtAudio print messages to stderr.
  adac.showWarnings( true );

  // Set the same number of channels for both input and output.
  unsigned int bufferFrames = 512;
  RtAudio::StreamParameters iParams, oParams;
  iParams.nChannels = channels;
  iParams.firstChannel = iOffset;
  oParams.nChannels = channels;
  oParams.firstChannel = oOffset;

  if ( iDevice == 0 )
    iParams.deviceId = adac.getDefaultInputDevice();
  else {
    if ( iDevice >= deviceIds.size() )
      iDevice = getDeviceIndex( adac.getDeviceNames(), true );
    iParams.deviceId = deviceIds[iDevice];
  }
  if ( oDevice == 0 )
    oParams.deviceId = adac.getDefaultOutputDevice();
  else {
    if ( oDevice >= deviceIds.size() )
      oDevice = getDeviceIndex( adac.getDeviceNames() );
    oParams.deviceId = deviceIds[oDevice];
  }
  
  RtAudio::StreamOptions options;
  //options.flags |= RTAUDIO_NONINTERLEAVED;

  bufferBytes = bufferFrames  * channels * sizeof( MY_TYPE );

  const unsigned int buffer_dump_size{fs * SECONDES * static_cast<unsigned int>(sizeof( MY_TYPE ))};
  MY_TYPE *input_buffer_dump = (MY_TYPE *) calloc(buffer_dump_size, sizeof(MY_TYPE));
  int ind_input_buff_dump{};

  MY_TYPE *output_buffer_dump = (MY_TYPE *) calloc(buffer_dump_size, sizeof(MY_TYPE));
  int ind_output_buff_dump{};


  // Lecture de la réponde impulsionelle
  double * impulse_response = load_impulse_response("../../impulse_response/impres"/*, &len_impulse_response*/);
  
  unsigned int convol_size{len_impulse_response + bufferFrames - 1};
  MY_TYPE *convol = (MY_TYPE *) calloc(convol_size, sizeof(MY_TYPE));

  CallbackData callbackData{
    bufferBytes, fs,
    input_buffer_dump, buffer_dump_size, ind_input_buff_dump,
    output_buffer_dump, buffer_dump_size, ind_output_buff_dump,
    impulse_response, len_impulse_response,
    convol, convol_size
  };


  // Ouvre le flux, si ce n'est pas possible, on cleanup le tout
  if ( adac.openStream( &oParams, &iParams, FORMAT, fs, &bufferFrames, &inout, (void *)&callbackData, &options ) ) {
    goto cleanup;
  }

  // Si le stream n'est ouvert, on clean up
  if ( adac.isStreamOpen() == false ) goto cleanup;

  // Test RtAudio functionality for reporting latency.
  std::cout << "\nStream latency = " << adac.getStreamLatency() << " frames" << std::endl;

  // Si on a une erreur ou un warning à l'ouverture du flux, on clean up
  if ( adac.startStream() ) goto cleanup;

  char input;
  std::cout << "\nRunning ... press <enter> to quit (buffer frames = " << bufferFrames << ").\n";

  // Là on attend que l'utilisateur appuie sur "Entrée" pour arreter le stream
  std::cin.get(input);


  // Stop the stream.
  if ( adac.isStreamRunning() )
    adac.stopStream();


  std::cout << "Writing buffer ...";
  if (buffer_dump_to_binary_file("dumpInput.bin", callbackData)) goto cleanup;
  if (buffer_dump_to_binary_file("dumpOutput.bin", callbackData)) goto cleanup;

  std::cout << "OK" << std::endl;

  free(input_buffer_dump);
  free(output_buffer_dump);

  
 cleanup:
  if ( adac.isStreamOpen() ) adac.closeStream();

  return 0;
}





int write_buff_dump(double* buff, const int n_buff, double* buff_dump, const int n_buff_dump, int* ind_dump) {

  int i = 0;
  for (i = 0; i < n_buff; i++) 
  {
    if (*ind_dump < n_buff_dump) {
      buff_dump[*ind_dump] = buff[i];
      (*ind_dump)++;
    } else {
      break;
    }
  }

  return i;
}

int buffer_dump_to_binary_file(const char *filename, CallbackData& callbackData) {

  FILE *f = fopen(filename, "wb");
  if ( f == NULL ) {
    std::cerr << "Cannot open file for writing" << std::endl;
    return 1;
  }

  if ( 1 != fwrite( callbackData.input_buffer_dump, callbackData.size_input_buffer_dump, 1, f ) ) {
    std::cerr << "Cannot write block in file" << std::endl;
    return 1;
  }

  fclose(f);

  return 0;
}


double * load_impulse_response(const char* filepath/*, unsigned int* len_impulse_response*/) {
  FILE *f = fopen(filepath, "rb");
  if ( f == NULL ) {
    std::cerr << "Cannot open file for reading" << std::endl;
    exit(1);
  }
  // Place la tête en fin de flux
  fseek(f, 0, SEEK_END);
  unsigned int len_impulse_response_octet{static_cast<unsigned int>(ftell(f))};
  // Calcul le nombre de valeur (double) présent dans ce fichier
  unsigned int number_of_double{static_cast<unsigned int>(len_impulse_response_octet / sizeof(double))};
  // Stockage dans la variable passé en paramètre
  // (*len_impulse_response) = number_of_double;
  // Remet la tete en début de fichier
  fseek(f, 0, SEEK_SET);


  // Alloue la mémoire pour le tableau stockant la réponse impulsionelle
  double *impulse_response = (double *) calloc(number_of_double, sizeof(double)); 

  if (number_of_double != fread(impulse_response, sizeof(double), number_of_double, f)) {
    std::cerr << "Cannot read correctly the stream" << std::endl;
    exit(1);
  }

  fclose(f);

  return impulse_response;
}











void usage( void ) {
  // Error function in case of incorrect command-line
  // argument specifications
  std::cout << "\nuseage: duplex N fs <iDevice> <oDevice> <iChannelOffset> <oChannelOffset>\n";
  std::cout << "    where N = number of channels,\n";
  std::cout << "    fs = the sample rate,\n";
  std::cout << "    iDevice = optional input device index to use (default = 0),\n";
  std::cout << "    oDevice = optional output device index to use (default = 0),\n";
  std::cout << "    iChannelOffset = an optional input channel offset (default = 0),\n";
  std::cout << "    and oChannelOffset = optional output channel offset (default = 0).\n\n";
  exit( 0 );
}

unsigned int getDeviceIndex( std::vector<std::string> deviceNames, bool isInput ) {
  unsigned int i;
  std::string keyHit;
  std::cout << '\n';
  for ( i=0; i<deviceNames.size(); i++ )
    std::cout << "  Device #" << i << ": " << deviceNames[i] << '\n';
  do {
    if ( isInput )
      std::cout << "\nChoose an input device #: ";
    else
      std::cout << "\nChoose an output device #: ";
    std::cin >> i;
  } while ( i >= deviceNames.size() );
  std::getline( std::cin, keyHit );  // used to clear out stdin
  return i;
}


// double get_process_time_windows(){
//     struct timeval time_now{};
//     gettimeofday(&time_now, nullptr);
//     time_t msecs_time = (time_now.tv_sec * 1000) + (time_now.tv_usec / 1000);
//     return (double) msecs_time;
// }


// #define WIN32_LEAN_AND_MEAN
// #include <Windows.h>
// #include <stdint.h> // portable: uint64_t   MSVC: __int64 

// MSVC defines this in winsock2.h!?
// typedef struct timeval {
//     long tv_sec;
//     long tv_usec;
// } timeval;

// int gettimeofday(struct timeval * tp, struct timezone * tzp)
// {
//     // Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
//     // This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
//     // until 00:00:00 January 1, 1970 
//     static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

//     SYSTEMTIME  system_time;
//     FILETIME    file_time;
//     uint64_t    time;

//     GetSystemTime( &system_time );
//     SystemTimeToFileTime( &system_time, &file_time );
//     time =  ((uint64_t)file_time.dwLowDateTime )      ;
//     time += ((uint64_t)file_time.dwHighDateTime) << 32;

//     tp->tv_sec  = (long) ((time - EPOCH) / 10000000L);
//     tp->tv_usec = (long) (system_time.wMilliseconds * 1000);
//     return 0;
// }