#pragma comment(lib, "fmod_vc.lib")

#include <string>
#include <sstream>
#include <ios>
#include <format>
#include <assert.h>
#include <windows.h>
#include <mfobjects.h>
#include <synchapi.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <propvarutil.h>
#include <propkey.h>

#include "include/fmod.hpp"

// Forward declaration
void RpgsPatchLog(const char* functionName, const std::string& message);

#if _DEBUG
#define PATCH_LOG(message) RpgsPatchLog(__func__, message);
#else
#define PATCH_LOG(message)
#endif

namespace mediaFoundation
{
    FILETIME GetCurrentFileTime()
    {
        FILETIME curFileTime;
        SYSTEMTIME sysTime;
        GetSystemTime(&sysTime);
        SystemTimeToFileTime(&sysTime, &curFileTime);
        return curFileTime;
    }

    class FmodReadStream : public IStream
    {
    public:
        FmodReadStream(FMOD_CODEC_STATE* inCodec) :
            codec(inCodec),
            referenceCount(1),
            currentReadPos(0)
        {
            streamStats.pwcsName = nullptr;
            streamStats.type = STGTY_STREAM;
            streamStats.cbSize.HighPart = 0;
            streamStats.cbSize.LowPart = codec->filesize;

            FILETIME curFileTime = GetCurrentFileTime();

            streamStats.mtime = curFileTime;
            streamStats.ctime = curFileTime;
            streamStats.atime = curFileTime;
            streamStats.grfMode = STGM_READ;
            streamStats.grfLocksSupported = 0;
        }

        virtual HRESULT QueryInterface(REFIID riid, void** returnObj) override
        {
            if (riid == IID_IStream || riid == IID_ISequentialStream || riid == IID_IUnknown)
            {
                *returnObj = this;
                AddRef();
                return S_OK;
            }
            else
            {
                return E_NOINTERFACE;
            }
        }

        virtual ULONG AddRef() override
        {
            referenceCount++;
            return 0;
        }

        virtual ULONG Release() override
        {
            referenceCount--;
            if (referenceCount == 0)
            {
                delete this;
            }
            return 0;
        }

        virtual HRESULT Read(void* buffer, ULONG bytesToRead, ULONG* bytesRead) override
        {
            unsigned int bytesReadAsInt = 0;
            FMOD_RESULT readResult = codec->fileread(codec->filehandle, buffer, bytesToRead, &bytesReadAsInt, nullptr);

            if (bytesRead != nullptr)
            {
                *bytesRead = bytesReadAsInt;
            }

            currentReadPos += bytesReadAsInt;
            if (readResult == FMOD_OK)
            {
                return S_OK;
            }
            else if (readResult == FMOD_ERR_FILE_EOF)
            {
                PATCH_LOG("Reached end-of-file.");
                return S_FALSE;
            }
            else
            {
                return STG_E_INVALIDPOINTER;
            }
        }

        virtual HRESULT Write(const void* buffer, ULONG bytesToWrite, ULONG* bytesWritten) override
        {
            // Read-only
            if (bytesWritten != nullptr)
            {
                bytesWritten = 0;
            }
            return S_OK;
        }

        virtual HRESULT Clone(IStream** newStream) override
        {
            // Don't even want to think about cajoling FMOD into giving us a new handle for the same file
            newStream = nullptr;
            return STG_E_INVALIDFUNCTION;
        }

        virtual HRESULT Commit(DWORD commitFlags) override
        {
            // Read-only, so we don't need to do anything
            return S_OK;
        }

        virtual HRESULT CopyTo(IStream* otherStream, ULARGE_INTEGER bytesToCopy, ULARGE_INTEGER* bytesRead, ULARGE_INTEGER* bytesWritten) override
        {
            if (bytesToCopy.HighPart > 0)
            {
                // FMOD codec fileread and IStream::Write don't support 64-bit sizes
                return STG_E_MEDIUMFULL;
            }

            FmodReadStream* otherFmodStream = dynamic_cast<FmodReadStream*>(otherStream);
            if (otherFmodStream != nullptr)
            {
                // We can't write to another one of this stream
                return STG_E_INVALIDPOINTER;
            }

            std::byte* copyBuffer = new std::byte[bytesToCopy.LowPart];

            HRESULT copyResult = S_OK;

            unsigned int copyBytesRead = 0;
            unsigned int copyBytesWritten = 0;
            FMOD_RESULT readResult = codec->fileread(codec->filehandle, copyBuffer, bytesToCopy.LowPart, &copyBytesRead, nullptr);
            if (readResult == FMOD_OK || readResult == FMOD_ERR_FILE_EOF)
            {
                ULONG longWritten = 0;
                copyResult = otherStream->Write(copyBuffer, copyBytesRead, &longWritten);
                copyBytesWritten = longWritten;

                // put the seek head back where it was
                codec->fileseek(codec->filehandle, currentReadPos, nullptr);
            }
            else
            {
                copyResult = STG_E_MEDIUMFULL;
            }

            if (bytesRead != nullptr)
            {
                bytesRead->HighPart = 0;
                bytesRead->LowPart = copyBytesRead;
            }
            if (bytesWritten != nullptr)
            {
                bytesWritten->HighPart = 0;
                bytesWritten->LowPart = copyBytesWritten;
            }

            if (SUCCEEDED(copyResult))
            {
                PATCH_LOG("Successful stream copy.");
            }
            else
            {
                PATCH_LOG(std::format("Stream copy failed!  Read error: {}; Write error: {}", static_cast<unsigned int>(readResult), copyResult));
            }

            delete[] copyBuffer;
            return copyResult;
        }

        virtual HRESULT LockRegion(ULARGE_INTEGER offset, ULARGE_INTEGER size, DWORD lockType) override
        {
            return STG_E_INVALIDFUNCTION;
        }

        virtual HRESULT Revert() override
        {
            return S_OK;
        }

        virtual HRESULT Seek(LARGE_INTEGER seekMove, DWORD seekRelativeType, ULARGE_INTEGER* newPosition) override
        {
            if (seekMove.HighPart > 0)
            {
                // Can't handle 64-bit offsets
                newPosition = nullptr;
                return STG_E_INVALIDFUNCTION;
            }

            unsigned int newPos = currentReadPos;

            switch (seekRelativeType)
            {
            case STREAM_SEEK_SET:
                {
                    newPos = seekMove.LowPart;
                    break;
                }
            case STREAM_SEEK_CUR:
                {
                    newPos += seekMove.LowPart;
                    break;
                }
            case STREAM_SEEK_END:
                {
                    newPos = codec->filesize - seekMove.LowPart;
                    break;
                }
            default:
                {
                    newPosition = nullptr;
                    return STG_E_INVALIDFUNCTION;
                }
            }

            if (newPos > codec->filesize)
            {
                newPosition = nullptr;
                return STG_E_INVALIDFUNCTION;
            }

            FMOD_RESULT seekResult = codec->fileseek(codec->filehandle, newPos, nullptr);
            if (seekResult != FMOD_OK)
            {
                PATCH_LOG(std::format("Seek operation failed: {}", static_cast<unsigned int>(seekResult)));
                return STG_E_INVALIDFUNCTION;
            }

            currentReadPos = newPos;

            if (newPosition != nullptr)
            {
                newPosition->HighPart = 0;
                newPosition->LowPart = newPos;
            }
            return S_OK;
        }

        virtual HRESULT SetSize(ULARGE_INTEGER newSize) override
        {
            if (newSize.HighPart > 0)
            {
                // We don't want Windows thinking we can support larger sizes than we actually can
                return STG_E_MEDIUMFULL;
            }

            // We don't actually have memory allocated for ourselves, so just pretend we did it.
            return S_OK;
        }

        virtual HRESULT Stat(STATSTG* outStats, DWORD statFlag) override
        {
            *outStats = streamStats;

            if (statFlag | STATFLAG_NONAME)
            {
                outStats->pwcsName = nullptr;
            }

            return S_OK;
        }

        virtual HRESULT UnlockRegion(ULARGE_INTEGER offset, ULARGE_INTEGER size, DWORD lockType) override
        {
            return STG_E_INVALIDFUNCTION;
        }

    private:
        FMOD_CODEC_STATE* codec;
        ULONG referenceCount;
        unsigned int currentReadPos;
        STATSTG streamStats;
    };

    class MfObjects
    {
    public:
        MfObjects() :
            fmodStream(nullptr),
            mfFmodStream(nullptr),
            mfResolver(nullptr),
            mfMedia(nullptr),
            mfReader(nullptr),
            mfBuffer(nullptr),
            lastReadTimestamp(0),
            currentBufferPos(0)
        { }

        virtual ~MfObjects()
        {
            if (mfBuffer != nullptr)
            {
                mfBuffer->Release();
            }
            if (mfReader != nullptr)
            {
                mfReader->Release();
            }
            if (mfMedia != nullptr)
            {
                mfMedia->Release();
            }
            if (mfResolver != nullptr)
            {
                mfResolver->Release();
            }
            if (mfFmodStream != nullptr)
            {
                mfFmodStream->Release();
            }
            if (fmodStream != nullptr)
            {
                fmodStream->Release();
            }
        }

        FmodReadStream* fmodStream;
        IMFByteStream* mfFmodStream;
        IMFSourceResolver* mfResolver;
        IMFMediaSource* mfMedia;
        IMFSourceReader* mfReader;
        IMFMediaBuffer* mfBuffer;

        LONGLONG lastReadTimestamp;
        unsigned int currentBufferPos;
    };

    HRESULT ConfigureAudioStream(IMFSourceReader* reader)
    {
        IMFMediaType* partialAudioType = nullptr;

        HRESULT result = reader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
        if (SUCCEEDED(result))
        {
            result = reader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
        }

        if (SUCCEEDED(result))
        {
            result = MFCreateMediaType(&partialAudioType);
        }

        if (SUCCEEDED(result))
        {
            result = partialAudioType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        }

        if (SUCCEEDED(result))
        {
            result = partialAudioType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        }

        if (SUCCEEDED(result))
        {
            // setting partial type on the reader will load the correct decoder
            result = reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, partialAudioType);
        }

        if (FAILED(result))
        {
            PATCH_LOG(std::format("Failed to configure audio stream: {}", result));
        }

        if (partialAudioType != nullptr)
        {
            partialAudioType->Release();
        }

        return result;
    }

    /*
    void FillOutMetadata(FMOD_CODEC_STATE* codec, MfObjects* mfObjects)
    {
        void* voidSource = nullptr;
        void* voidStore = nullptr;
        void* voidMetadata = nullptr;

        IMFMediaSource* mediaSource = nullptr;
        IPropertyStore* propertyStore = nullptr;

        HRESULT winLibResult = mfObjects->mfReader->GetServiceForStream(MF_SOURCE_READER_FIRST_AUDIO_STREAM, MF_MEDIASOURCE_SERVICE, IID_IMFMediaSource, &voidSource);
        if (SUCCEEDED(winLibResult))
        {
            mediaSource = static_cast<IMFMediaSource*>(voidSource);
            winLibResult = MFGetService(mediaSource, MF_PROPERTY_HANDLER_SERVICE, IID_IPropertyStore, &voidStore);
        }

        DWORD metadataPropCount = 0;
        if (SUCCEEDED(winLibResult))
        {
            propertyStore = static_cast<IPropertyStore*>(voidStore);

            winLibResult = propertyStore->GetCount(&metadataPropCount);
        }

        if (SUCCEEDED(winLibResult))
        {
            for (DWORD i = 0; i < metadataPropCount; i++)
            {
                PROPERTYKEY key;
                winLibResult = propertyStore->GetAt(i, &key);
                if (FAILED(winLibResult))
                {
                    break;
                }

                PROPVARIANT valueVariant;
                winLibResult = propertyStore->GetValue(key, &valueVariant);
                if (FAILED(winLibResult))
                {
                    break;
                }

                const char* metadatumName = nullptr;
                void* metadatum = nullptr;
                unsigned int metadatumLength = 0;
                FMOD_TAGDATATYPE metadatumType = FMOD_TAGDATATYPE_MAX;

                if (key == PKEY_Title)
                {
                    metadatumName = "Title";
                    PropVariantToStringAlloc(valueVariant, reinterpret_cast<PWSTR*>(&metadatum));
                    metadatumType = FMOD_TAGDATATYPE_STRING_UTF16;
                }
                else if (key == PKEY_Music_Artist)
                {

                }

                char* nonConstName = nullptr;
                if (metadatumName != nullptr)
                {
                    // metadata() wants a non-const for the name, and allocation in each case is fluff.
                    const size_t nameLen = sizeof(metadatumName);
                    nonConstName = new char[nameLen];
                    std::memcpy(nonConstName, metadatumName, nameLen);
                }

                codec->metadata(codec, FMOD_TAGTYPE_USER, nonConstName, metadatum, metadatumLength, metadatumType, 1);

                PropVariantClear(&valueVariant);
            }
        }

        if (mediaSource != nullptr)
        {
            mediaSource->Release();
        }
        if (propertyStore != nullptr)
        {
            propertyStore->Release();
        }
    }
    */

    bool FindMimeType(FMOD_CODEC_STATE* codec, WCHAR* outMime, size_t mimeMaxLength)
    {
        // Endianness means that I have to write these out individually rather than packaging them into larger ints
        static const uint8_t m4aSig[] = {0x00, 0x00, 0x00, 0x20, 0x66, 0x74, 0x79, 0x70, 0x4d, 0x34, 0x41, 0x20};
        static const WCHAR m4aMime[] = L"audio/mp4";
        static const uint8_t wmaSig[] = {0x30, 0x26, 0xb2, 0x75, 0x8e, 0x66, 0xcf, 0x11, 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c};
        static const WCHAR wmaMime[] = L"audio/x-ms-wma";

        assert(sizeof(m4aMime) <= mimeMaxLength);
        assert(sizeof(wmaMime) <= mimeMaxLength);

        bool success = false;

        uint8_t* signatureBuffer = new uint8_t[32];
        unsigned int bytesRead = 0;
        FMOD_RESULT readResult = codec->fileread(codec->filehandle, signatureBuffer, 32, &bytesRead, nullptr);

        if (readResult == FMOD_OK)
        {
            if (std::memcmp(signatureBuffer, m4aSig, sizeof(m4aSig)) == 0 || std::memcmp(signatureBuffer + 4, m4aSig + 1, sizeof(m4aSig) - sizeof(UINT32)) == 0)
            {
                // It's an M4A!
                // M4A files can have slightly different signatures.
                std::memcpy(outMime, m4aMime, sizeof(m4aMime));
                success = true;
            }
            else if (std::memcmp(signatureBuffer, wmaSig, sizeof(wmaSig)) == 0)
            {
                // It's a WMA!
                std::memcpy(outMime, wmaMime, sizeof(wmaMime));
                success = true;
            }
#if _DEBUG
            else
            {
                std::stringstream signatureInHex;
                for (unsigned int i = 0; i < 32; i++)
                {
                    signatureInHex << std::format("{:02x}", signatureBuffer[i]);
                }
                PATCH_LOG(std::format("Could not find signature to match {}", signatureInHex.str()));
            }
#endif
        }
        else
        {
            PATCH_LOG("Could not read from audio file!");
        }

        // put the file back
        codec->fileseek(codec->filehandle, 0, nullptr);
        return success;
    }

    FMOD_RESULT F_CALLBACK open(FMOD_CODEC_STATE* codec, FMOD_MODE userMode, FMOD_CREATESOUNDEXINFO* userExInfo)
    {
        // Make sure, for sanity's sake, that we're starting at the beginning of the file.
        codec->fileseek(codec->filehandle, 0, nullptr);

        WCHAR* mimeType = new WCHAR[16];
        if (!FindMimeType(codec, mimeType, sizeof(WCHAR[16])))
        {
            PATCH_LOG("No matching MIME.");
            delete[] mimeType;
            return FMOD_ERR_FORMAT;
        }

#if _DEBUG
        char* mimeTypeChar = new char[32];
        WideCharToMultiByte(CP_UTF8, 0, mimeType, 16, mimeTypeChar, 32, nullptr, nullptr);
        PATCH_LOG(std::format("MIME found: {}", mimeTypeChar));
#endif

        codec->waveformatversion = FMOD_CODEC_WAVEFORMAT_VERSION;

        MfObjects* mfObjects = new MfObjects();

        mfObjects->fmodStream = new FmodReadStream(codec);

        FMOD_RESULT returnResult = FMOD_OK;

        HRESULT winLibResult = MFCreateMFByteStreamOnStream(mfObjects->fmodStream, &(mfObjects->mfFmodStream));

        if (SUCCEEDED(winLibResult))
        {
            PATCH_LOG("MF byte stream created.");

            // Need to tell the byte stream what kind of format it is
            IMFAttributes* streamAttributes = nullptr;
            winLibResult = mfObjects->mfFmodStream->QueryInterface<IMFAttributes>(&streamAttributes);

            if (SUCCEEDED(winLibResult))
            {
                winLibResult = streamAttributes->SetString(MF_BYTESTREAM_CONTENT_TYPE, mimeType);
            }
        }

        if (SUCCEEDED(winLibResult))
        {
            PATCH_LOG("MIME type given to byte stream attributes.");

            winLibResult = MFCreateSourceResolver(&(mfObjects->mfResolver));
        }

        if (SUCCEEDED(winLibResult))
        {
            PATCH_LOG("Source resolver ready.");

            MF_OBJECT_TYPE objType;
            IUnknown* unknownMedia;
            winLibResult = mfObjects->mfResolver->CreateObjectFromByteStream(mfObjects->mfFmodStream, nullptr, MF_RESOLUTION_MEDIASOURCE | MF_RESOLUTION_READ, nullptr, &objType, &unknownMedia);

            if (SUCCEEDED(winLibResult))
            {
                winLibResult = unknownMedia->QueryInterface(IID_PPV_ARGS(&(mfObjects->mfMedia)));

                unknownMedia->Release();
            }
        }

        if (SUCCEEDED(winLibResult))
        {
            PATCH_LOG("Media source prepared.");

            winLibResult = MFCreateSourceReaderFromMediaSource(mfObjects->mfMedia, nullptr, &(mfObjects->mfReader));
        }

        if (SUCCEEDED(winLibResult))
        {
            PATCH_LOG("Source reader created.");

            winLibResult = ConfigureAudioStream(mfObjects->mfReader);
        }

        if (SUCCEEDED(winLibResult))
        {
            PATCH_LOG("Audio stream configured.  Open successful.");

            codec->plugindata = mfObjects;

            // Give metadata to FMOD
            // This isn't vital to functionality, so this function will still give FMOD_OK if we get here,
            // regardless of whether we actually successfully get metadata.
            //FillOutMetadata(codec, mfObjects);
        }
        else
        {
            PATCH_LOG("File format invalid.");

            delete mfObjects;
            returnResult = FMOD_ERR_FILE_BAD;
        }

        delete[] mimeType;

        return returnResult;
    }

    FMOD_RESULT F_CALLBACK close(FMOD_CODEC_STATE* codec)
    {
        MfObjects* mfObjects = static_cast<MfObjects*>(codec->plugindata);
        if (mfObjects != nullptr)
        {
            delete mfObjects;
        }

        PATCH_LOG("Audio file closed.");

        return FMOD_OK;
    }

    unsigned int ConvertFrom100nsTimestamp(LONGLONG timestamp100ns, FMOD_TIMEUNIT timeUnit, IMFMediaType* audioType)
    {
        const UINT32 bytesPerSec = MFGetAttributeUINT32(audioType, MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 0);

        switch (timeUnit)
        {
        case FMOD_TIMEUNIT_MS:
            {
                unsigned int msTimestamp = (unsigned int)(timestamp100ns / 10000);
                //PATCH_LOG(std::format("{}00 nanoseconds is {} milliseconds.", timestamp100ns, msTimestamp));
                return msTimestamp;
                break;
            }
        case FMOD_TIMEUNIT_PCM:
            {
                // Divide by channels and bytes-per-sample in addition to the 10 million for the whole 100ns thing
                const UINT32 channels = MFGetAttributeUINT32(audioType, MF_MT_AUDIO_NUM_CHANNELS, 0);
                const UINT32 bits = MFGetAttributeUINT32(audioType, MF_MT_AUDIO_BITS_PER_SAMPLE, 0);

                const UINT64 bytesPerSample = channels * bits / 8;

                unsigned int sampleTimestamp = (unsigned int)(timestamp100ns * bytesPerSec / (bytesPerSample * 10000000));
                //PATCH_LOG(std::format("{}00 nanoseconds is {} samples ({} channels, {} bits per sample).", timestamp100ns, sampleTimestamp, channels, bits));
                return sampleTimestamp;
                break;
            }
        case FMOD_TIMEUNIT_PCMBYTES:
            {
                unsigned int byteTimestamp = (unsigned int)(timestamp100ns * bytesPerSec / 10000000);
                //PATCH_LOG(std::format("{}00 nanoseconds is {} bytes.", timestamp100ns, byteTimestamp));
                return byteTimestamp;
                break;
            }
        }

        return -1;
    }

    LONGLONG ConvertTo100nsTimestamp(unsigned int time, FMOD_TIMEUNIT timeUnit, IMFMediaType* mediaType)
    {
        const UINT32 bytesPerSec = MFGetAttributeUINT32(mediaType, MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 0);

        INT64 positionIn100ns = 0;
        // Convert desired position to 100ns units
        switch (timeUnit)
        {
        case FMOD_TIMEUNIT_MS:
            {
                positionIn100ns = (INT64)time * 10000;
                break;
            }
        case FMOD_TIMEUNIT_PCM:
            {
                // Multiply by channels and bytes-per-sample in addition to the 10 million for the whole 100ns thing
                const UINT32 channels = MFGetAttributeUINT32(mediaType, MF_MT_AUDIO_NUM_CHANNELS, 0);
                const UINT32 bits = MFGetAttributeUINT32(mediaType, MF_MT_AUDIO_BITS_PER_SAMPLE, 0);

                const UINT32 bytesPerSample = channels * bits / 8;

                positionIn100ns = (INT64)time * 10000000 * bytesPerSample / bytesPerSec;
                break;
            }
        case FMOD_TIMEUNIT_PCMBYTES:
            {
                positionIn100ns = (INT64)time * 10000000 / bytesPerSec;
                break;
            }
        default:
            return -1;
        }

        return positionIn100ns;
    }

    FMOD_RESULT F_CALLBACK getLength(FMOD_CODEC_STATE* codec, unsigned int* length, FMOD_TIMEUNIT timeUnit)
    {
        MfObjects* mfObjects = static_cast<MfObjects*>(codec->plugindata);
        if (mfObjects == nullptr || mfObjects->mfReader == nullptr)
        {
            PATCH_LOG("Invalid plugin data in codec state!");

            return FMOD_ERR_PLUGIN;
        }

        if (timeUnit == FMOD_TIMEUNIT_RAWBYTES)
        {
            PROPVARIANT sizeVariant;
            HRESULT lengthResult = mfObjects->mfReader->GetPresentationAttribute((DWORD)MF_SOURCE_READER_MEDIASOURCE, MF_PD_TOTAL_FILE_SIZE, &sizeVariant);
            if (FAILED(lengthResult))
            {
                PATCH_LOG("Could not get filesize attribute from the source reader.");
                return FMOD_ERR_PLUGIN;
            }

            lengthResult = PropVariantToUInt32(sizeVariant, length);
            if (FAILED(lengthResult))
            {
                PATCH_LOG("Filesize attributed did not convert to Int32.");
                return FMOD_ERR_PLUGIN;
            }

            return FMOD_OK;
        }
        else if (timeUnit != FMOD_TIMEUNIT_MS && timeUnit != FMOD_TIMEUNIT_PCM && timeUnit != FMOD_TIMEUNIT_PCMBYTES)
        {
            // Don't need to support other time unit types because they apply to sequences or DSPs
            return FMOD_ERR_PLUGIN;
        }

        IMFMediaType* audioType = nullptr;
        HRESULT winLibResult = mfObjects->mfReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &audioType);
        if (FAILED(winLibResult))
        {
            PATCH_LOG("Failed to get audio type from the source reader.");
            return FMOD_ERR_PLUGIN;
        }

        PROPVARIANT durationVariant;
        winLibResult = mfObjects->mfReader->GetPresentationAttribute((DWORD)MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &durationVariant);
        if (FAILED(winLibResult))
        {
            PATCH_LOG("Failed to get duration attribute from the source reader.");
            return FMOD_ERR_PLUGIN;
        }

        // For some freaking reason, the duration attribute counts in hundreds of nanoseconds.
        INT64 trueDurationIn100ns = 0;
        winLibResult = PropVariantToInt64(durationVariant, &trueDurationIn100ns);
        if (FAILED(winLibResult))
        {
            PATCH_LOG("Failed to convert duration in hundreds of nanoseconds to an Int64.");
            return FMOD_ERR_PLUGIN;
        }

        *length = ConvertFrom100nsTimestamp(trueDurationIn100ns, timeUnit, audioType);

        return FMOD_OK;
    }

    FMOD_RESULT F_CALLBACK setPosition(FMOD_CODEC_STATE* codec, int subsound, unsigned int position, FMOD_TIMEUNIT timeUnit)
    {
        MfObjects* mfObjects = static_cast<MfObjects*>(codec->plugindata);
        if (mfObjects == nullptr || mfObjects->mfReader == nullptr)
        {
            PATCH_LOG("Invalid plugin data in codec state!");

            return FMOD_ERR_PLUGIN;
        }

        HRESULT winLibResult = S_OK;

        IMFMediaType* audioType = nullptr;
        winLibResult = mfObjects->mfReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &audioType);
        if (FAILED(winLibResult))
        {
            PATCH_LOG("Could not get media type from source reader.");
            return FMOD_ERR_PLUGIN;
        }

        LONGLONG positionIn100ns = ConvertTo100nsTimestamp(position, timeUnit, audioType);

        if (positionIn100ns < 0)
        {
            return FMOD_ERR_PLUGIN;
        }

        PROPVARIANT positionVariant;
        winLibResult = InitPropVariantFromInt64(positionIn100ns, &positionVariant);

        if (SUCCEEDED(winLibResult))
        {
            winLibResult = mfObjects->mfReader->SetCurrentPosition(GUID_NULL, positionVariant);
            PropVariantClear(&positionVariant);

            mfObjects->lastReadTimestamp = positionIn100ns;
        }

        return SUCCEEDED(winLibResult) ? FMOD_OK : FMOD_ERR_PLUGIN;
    }

    FMOD_RESULT F_CALLBACK getPosition(FMOD_CODEC_STATE* codec, unsigned int* position, FMOD_TIMEUNIT timeUnit)
    {
        MfObjects* mfObjects = static_cast<MfObjects*>(codec->plugindata);
        if (mfObjects == nullptr || mfObjects->mfReader == nullptr)
        {
            PATCH_LOG("Invalid plugin data in codec state!");

            return FMOD_ERR_PLUGIN;
        }

        IMFMediaType* audioType = nullptr;
        HRESULT winLibResult = mfObjects->mfReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &audioType);
        if (FAILED(winLibResult))
        {
            PATCH_LOG("Could not get media type from source reader.");
            return FMOD_ERR_PLUGIN;
        }

        *position = ConvertFrom100nsTimestamp(mfObjects->lastReadTimestamp, timeUnit, audioType);

        return FMOD_OK;
    }

    FMOD_RESULT F_CALLBACK read(FMOD_CODEC_STATE* codec, void* buffer, unsigned int samplesRequested, unsigned int* samplesRead)
    {
        MfObjects* mfObjects = static_cast<MfObjects*>(codec->plugindata);
        if (mfObjects == nullptr || mfObjects->mfReader == nullptr)
        {
            PATCH_LOG("Invalid plugin data in codec state!");

            return FMOD_ERR_PLUGIN;
        }

        IMFMediaType* audioType = nullptr;
        HRESULT winLibResult = mfObjects->mfReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &audioType);
        if (FAILED(winLibResult))
        {
            PATCH_LOG("Failed to get audio type from the source reader.");
            return FMOD_ERR_PLUGIN;
        }

        *samplesRead = 0;

        const UINT32 channels = MFGetAttributeUINT32(audioType, MF_MT_AUDIO_NUM_CHANNELS, 0);
        const UINT32 bits = MFGetAttributeUINT32(audioType, MF_MT_AUDIO_BITS_PER_SAMPLE, 0);

        const UINT32 bytesPerSample = channels * bits / 8;

        winLibResult = S_OK;
        FMOD_RESULT returnResult = FMOD_OK;

        BYTE* rawAudioData = nullptr;
        DWORD mfBufferSize = 0;
        while (*samplesRead < samplesRequested)
        {
            if (mfObjects->mfBuffer == nullptr)
            {
                // IMFSourceReader can give more data in one go than FMOD will ever ask for, and we don't have fine enough
                // granularity with seeking to adjust for that.  Instead, we put the MF sample into a buffer and read from
                // that gradually.  When that runs out, then we ask for a new sample from the source reader.

                mfObjects->currentBufferPos = 0;

                DWORD sampleReadFlags = 0;
                IMFSample* sample = nullptr;
                winLibResult = mfObjects->mfReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &sampleReadFlags, &(mfObjects->lastReadTimestamp), &sample);

                if (FAILED(winLibResult))
                {
                    PATCH_LOG(std::format("Failed to read sample: {}", winLibResult));
                    returnResult = FMOD_ERR_PLUGIN;
                    break;
                }

                if (sampleReadFlags & MF_SOURCE_READERF_ENDOFSTREAM)
                {
                    PATCH_LOG("End of stream.");
                    break;
                }

                DWORD sampleLength = 0;
                winLibResult = sample->GetTotalLength(&sampleLength);
                if (FAILED(winLibResult))
                {
                    PATCH_LOG(std::format("Failed to get sample length: {}", winLibResult));
                    returnResult = FMOD_ERR_PLUGIN;
                    break;
                }

                winLibResult = MFCreateMemoryBuffer(sampleLength, &(mfObjects->mfBuffer));
                if (FAILED(winLibResult))
                {
                    PATCH_LOG(std::format("Failed to initialize buffer: {}", winLibResult));
                    returnResult = FMOD_ERR_PLUGIN;
                    break;
                }

                // The intuitive thing to do here would be to, rather than separately initializing a buffer,
                // just call IMFSample::ConvertToContiguousBuffer().  However, since that can just point
                // directly to the internal buffer of the sample, the sample would need to stay valid.
                // However, samples really don't like sticking around, causing a crash inside Media Foundation.
                winLibResult = sample->CopyToBuffer(mfObjects->mfBuffer);
                if (FAILED(winLibResult))
                {
                    PATCH_LOG(std::format("Failed to copy sample to buffer: {}", winLibResult));
                    returnResult = FMOD_ERR_PLUGIN;
                    break;
                }

                sample->Release();
            }

            winLibResult = mfObjects->mfBuffer->Lock(&rawAudioData, nullptr, &mfBufferSize);
            if (FAILED(winLibResult))
            {
                PATCH_LOG(std::format("Failed to lock sample buffer: {}", winLibResult));
                returnResult = FMOD_ERR_PLUGIN;
                break;
            }

            // Actual copy to FMOD's buffer
            unsigned int maxSamplesToRead = samplesRequested - *samplesRead;
            unsigned int bytesToCopy = min(maxSamplesToRead * bytesPerSample, mfBufferSize);
            std::memcpy(static_cast<BYTE*>(buffer) + (*samplesRead * bytesPerSample), rawAudioData + mfObjects->currentBufferPos, bytesToCopy);

            winLibResult = mfObjects->mfBuffer->Unlock();
            if (FAILED(winLibResult))
            {
                PATCH_LOG("Failed to unlock media buffer!");
            }
            rawAudioData = nullptr;

            // Update timestamps
            mfObjects->currentBufferPos += bytesToCopy;
            *samplesRead += bytesToCopy / bytesPerSample;
            mfObjects->lastReadTimestamp += ConvertTo100nsTimestamp(bytesToCopy, FMOD_TIMEUNIT_PCMBYTES, audioType);

            // If we've reached the end of the current buffer, kill it
            if (mfObjects->currentBufferPos == mfBufferSize)
            {
                mfObjects->mfBuffer->Release();
                mfObjects->mfBuffer = nullptr;
            }
        }
        if (audioType != nullptr)
        {
            audioType->Release();
        }

        return returnResult;
    }

    FMOD_RESULT F_CALLBACK soundCreated(FMOD_CODEC_STATE* codec, int subsound, FMOD_SOUND* sound)
    {
        // Don't need to do anything here
        return FMOD_OK;
    }

    FMOD_RESULT F_CALLBACK getWaveFormat(FMOD_CODEC_STATE* codec, int index, FMOD_CODEC_WAVEFORMAT* waveFormat)
    {
        MfObjects* mfObjects = static_cast<MfObjects*>(codec->plugindata);
        if (mfObjects == nullptr || mfObjects->mfReader == nullptr || mfObjects->mfFmodStream == nullptr)
        {
            PATCH_LOG("Invalid plugin data in codec state!");

            return FMOD_ERR_PLUGIN;
        }

        IMFMediaType* audioType = nullptr;
        HRESULT winLibResult = mfObjects->mfReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &audioType);
        if (FAILED(winLibResult))
        {
            PATCH_LOG("Could not get media type from source reader.");
            return FMOD_ERR_PLUGIN;
        }

        // Get base data
        const UINT32 channels = MFGetAttributeUINT32(audioType, MF_MT_AUDIO_NUM_CHANNELS, 0);
        const UINT32 bits = MFGetAttributeUINT32(audioType, MF_MT_AUDIO_BITS_PER_SAMPLE, 0);
        const UINT32 frequency = MFGetAttributeUINT32(audioType, MF_MT_AUDIO_SAMPLES_PER_SECOND, 0);
        const UINT32 blockSize = MFGetAttributeUINT32(audioType, MF_MT_AUDIO_SAMPLES_PER_BLOCK, 0);
        const UINT32 channelMask = MFGetAttributeUINT32(audioType, MF_MT_AUDIO_CHANNEL_MASK, 0);

        //PATCH_LOG(std::format("{} Hz, {} channels, {} bits", frequency, channels, bits));

        unsigned int samples = 0;
        getLength(codec, &samples, FMOD_TIMEUNIT_PCM);
        unsigned int bytes = 0;
        getLength(codec, &bytes, FMOD_TIMEUNIT_RAWBYTES);

        // Apply data
        switch (bits)
        {
        case 8:
            {
                waveFormat->format = FMOD_SOUND_FORMAT_PCM8;
                break;
            }
        case 16:
            {
                waveFormat->format = FMOD_SOUND_FORMAT_PCM16;
                break;
            }
        case 24:
            {
                waveFormat->format = FMOD_SOUND_FORMAT_PCM24;
                break;
            }
        case 32:
            {
                waveFormat->format = FMOD_SOUND_FORMAT_PCM32;
                break;
            }
        default:
            waveFormat->format = FMOD_SOUND_FORMAT_NONE;
        }

        waveFormat->channels = channels;
        waveFormat->frequency = frequency;
        waveFormat->lengthbytes = bytes;
        waveFormat->lengthpcm = samples;
        waveFormat->pcmblocksize = blockSize;

        if (channelMask & SPEAKER_FRONT_LEFT)
        {
            waveFormat->channelmask |= FMOD_CHANNELMASK_FRONT_LEFT;
        }
        if (channelMask & SPEAKER_FRONT_RIGHT)
        {
            waveFormat->channelmask |= FMOD_CHANNELMASK_FRONT_RIGHT;
        }
        if (channelMask & SPEAKER_FRONT_CENTER)
        {
            waveFormat->channelmask |= FMOD_CHANNELMASK_FRONT_CENTER;
        }
        if (channelMask & SPEAKER_LOW_FREQUENCY)
        {
            waveFormat->channelmask |= FMOD_CHANNELMASK_LOW_FREQUENCY;
        }
        if (channelMask & SPEAKER_BACK_LEFT)
        {
            waveFormat->channelmask |= FMOD_CHANNELMASK_BACK_LEFT;
        }
        if (channelMask & SPEAKER_BACK_RIGHT)
        {
            waveFormat->channelmask |= FMOD_CHANNELMASK_BACK_RIGHT;
        }
        if (channelMask & SPEAKER_BACK_CENTER)
        {
            waveFormat->channelmask |= FMOD_CHANNELMASK_BACK_CENTER;
        }
        if (channelMask & SPEAKER_SIDE_LEFT)
        {
            waveFormat->channelmask |= FMOD_CHANNELMASK_SURROUND_LEFT;
        }
        if (channelMask & SPEAKER_SIDE_RIGHT)
        {
            waveFormat->channelmask |= FMOD_CHANNELMASK_SURROUND_RIGHT;
        }

        return FMOD_OK;
    }

    FMOD_CODEC_DESCRIPTION mfCodec = {
        "FMOD Win32 Media Foundation Codec",
        0x00010000,
        0,
        FMOD_TIMEUNIT_PCM | FMOD_TIMEUNIT_PCMBYTES | FMOD_TIMEUNIT_MS,
        &open,
        &close,
        &read,
        &getLength,
        &setPosition,
        &getPosition,
        &soundCreated,
        &getWaveFormat
    };
};

extern "C" {
    typedef void(__stdcall* FuncCallBack)(const char*, int);

    // C++ functions get name-mangled, so we need to export these via extern-C
    __declspec(dllexport) FMOD_CODEC_DESCRIPTION* F_CALL FMODGetCodecDescription();
    __declspec(dllexport) bool __stdcall RegisterLogCallback(FuncCallBack cb);
}

FMOD_CODEC_DESCRIPTION* FMODGetCodecDescription()
{
    return &mediaFoundation::mfCodec;
}

static FuncCallBack callbackInstance = nullptr;
bool RegisterLogCallback(FuncCallBack cb)
{
    if (cb != nullptr)
    {
        callbackInstance = cb;
#if _DEBUG
        static char registrationResponse[] = "Logging native-to-C# delegate registered.";
        callbackInstance(registrationResponse, sizeof(registrationResponse));
#endif
        return true;
    }
    else
    {
        return false;
    }
}

void RpgsPatchLog(const char* functionName, const std::string& message)
{
    if (callbackInstance != nullptr)
    {
        std::string formattedMessage = std::format("[FMOD-Win32-MF::{}] {}", functionName, message);
        callbackInstance(formattedMessage.c_str(), formattedMessage.length());
    }
    else
    {
        assert(false);
    }
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        HRESULT winLibResult = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (FAILED(winLibResult))
        {
            return FALSE;
        }

        winLibResult = MFStartup(MF_VERSION);
        if (FAILED(winLibResult))
        {
            return FALSE;
        }
    }
    else if (fdwReason == DLL_PROCESS_DETACH)
    {
        MFShutdown();
        CoUninitialize();
    }

    return TRUE;
}