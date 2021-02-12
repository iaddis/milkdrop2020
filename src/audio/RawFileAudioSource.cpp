

#include "VizController.h"

class RawFileAudioSource : public IAudioSource
{
    std::string mDescription;
public:
	RawFileAudioSource(FILE *file)
		:m_file(file)
	{
        mDescription = "RawFile";
	}
    
    virtual ~RawFileAudioSource()
    {
        fclose(m_file);
    }
    
    
    virtual const std::string &GetDescription() const override
    {
       return mDescription;
    }
    
    
    
    virtual void Reset() override
    {
        fseek(m_file, 0, SEEK_SET);
    }
    


    float Convert(uint8_t x)
    {
        return ((x ^ 128) - 128.0f) / 256.0f;
    }
    
    virtual void ReadAudioFrame(float dt, SampleBuffer<Sample> &samples) override
    {
        // read audio from source file
        uint8_t data[2][576];
        memset(data, 0x80, sizeof(data));
        
        if (fread(data, sizeof(data), 1, m_file) != 1) {
            fseek(m_file, 0, SEEK_SET);
        }
        
        samples.SetSampleRate(576 * 60);
        
        int size = 576;
        samples.clear();
        samples.reserve(size);
        for (int i=0; i < size; i++)
        {
            Sample s { Convert(data[0][i]), Convert(data[1][i])};
            samples.push_back(s);
        }
	}

protected:
    FILE *m_file;
};


IAudioSourcePtr OpenRawFileAudioSource(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
    {
        LogPrint("Could not open audio file: %s\n", path);
        return nullptr;
    }
    
    LogPrint("Audio file opened: %s\n", path);

    return std::make_shared<RawFileAudioSource>(file);
}
