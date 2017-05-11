
#include "MP3Reader.h"

#include "mpg123.h"


ssize_t MP3Reader::s_read(void * handle, void *buffer, size_t size) {
    return ((File*)handle)->read((unsigned char*)buffer, size);
}

off_t MP3Reader::s_lseek(void *handle, off_t offset, int whence) {
    switch (whence)
    {
        case SEEK_CUR:
            ((File*)handle)->seek(((File*)handle)->pos() + offset);
            break;
        case SEEK_END:
            ((File*)handle)->seek(((File*)handle)->length() + offset);
            break;
        default:
            ((File*)handle)->seek(offset);
    }
    return ((File*)handle)->pos();
}


void MP3Reader::s_cleanup(void *handle) {
    //Not necessary to close handle here
    //delete ((FileSystem*)handle);
}


AudioFile* MP3Reader::getRawAudio(FileData* filedata){

    mpg123_handle *mh;

    int channels, encoding, bitsPerSample;
    long rate;
    unsigned int size;
    size_t done;
    off_t numSamples;

    mpg123_init();

    int err = MPG123_OK;

    mh = mpg123_new(NULL, &err);
    if (mh == NULL || err != MPG123_OK)
    {
        return NULL;
    }

    mpg123_replace_reader_handle(mh, s_read, s_lseek, s_cleanup);

    if (mpg123_open_handle(mh, filedata) != MPG123_OK) {
        mpg123_delete(mh);
        return NULL;
    }

    if (mpg123_getformat(mh, &rate, &channels, &encoding) !=  MPG123_OK) {
        mpg123_delete(mh);
        return NULL;
    }

    mpg123_scan(mh);

    numSamples = mpg123_length(mh);
    bitsPerSample = mpg123_encsize(encoding) * 8;

    size = numSamples * channels * mpg123_encsize(encoding);

    int readchannels = 1;
    int mChannels = channels;
    if (channels > 1)
    {
        readchannels = 2;
        channels = 2;
    }
    float* mData = new float[numSamples * readchannels];

    void *data = malloc(size);

    err = mpg123_read(mh, (unsigned char*)data, size, &done );

    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();

    if (err == MPG123_OK || err == MPG123_DONE){

        FileData tempData;
        tempData.open((unsigned char*)data, size, false, true);

        int i, j;
        if (bitsPerSample == 8)
        {
            for (i = 0; i < numSamples; i++)
            {
                for (j = 0; j < channels; j++)
                {
                    if (j == 0)
                    {
                        mData[i] = ((signed)tempData.read8() - 128) / (float)0x80;
                    }
                    else
                    {
                        if (readchannels > 1 && j == 1)
                        {
                            mData[i + numSamples] = ((signed)tempData.read8() - 128) / (float)0x80;
                        }
                        else
                        {
                            tempData.read8();
                        }
                    }
                }
            }
        }
        else
        if (bitsPerSample == 16)
        {
            for (i = 0; i < numSamples; i++)
            {
                for (j = 0; j < channels; j++)
                {
                    if (j == 0)
                    {
                        mData[i] = ((signed short)tempData.read16()) / (float)0x8000;
                    }
                    else
                    {
                        if (readchannels > 1 && j == 1)
                        {
                            mData[i + numSamples] = ((signed short)tempData.read16()) / (float)0x8000;
                        }
                        else
                        {
                            tempData.read16();
                        }
                    }
                }
            }
        }

        FileData* data = new FileData((unsigned char*)mData, sizeof(float) * numSamples * readchannels);

        return new AudioFile(mChannels, bitsPerSample, numSamples, rate, data);
    }

    return NULL;

}