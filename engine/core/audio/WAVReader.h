

#ifndef WAVReader_h
#define WAVReader_h

#include "AudioReader.h"

class WAVReader: public AudioReader{
    
public:
    AudioFile* getRawAudio(FileData* filedata);
};

#endif /* WAVReader_h */
