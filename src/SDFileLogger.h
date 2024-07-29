#ifndef SD_FILE_LOGGER_H_
#define SD_FILE_LOGGER_H_

#include "Arduino.h"
#include "ArduinoLogger.h"
#include "SdFat.h"
#include "internal/circular_buffer.hpp"

/** SD File Buffer
 *
 * Logs to a file on the SD card.
 *
 * This class uses the SdFat Arduino Library.
 *
 *	@code
 *	using PlatformLogger =
 *		PlatformLogger_t<SDFileLogger>;
 *  @endcode
 *
 * @ingroup LoggingSubsystem
 */
class SDFileLogger final : public LoggerBase
{
  private:
	static constexpr size_t BUFFER_SIZE = 2048;
	static constexpr size_t READY_BUFFER_SIZE = 512;

  public:
	/// Default constructor
	SDFileLogger() : LoggerBase() {}

	/// Default destructor
	~SDFileLogger() noexcept = default;

	size_t size() const noexcept final
	{
		return file_.size();
	}

	size_t capacity() const noexcept final
	{
		// size in blocks * bytes per block (512 Bytes = 2^9)
		return fs_ ? fs_->card()->sectorCount() << 9 : 0;
	}

	void log_customprefix() noexcept final
	{
		print("[%d ms] ", millis());
	}

	void begin()
	{
		logToFile_ = false;
	}

	void begin(SdFs* sd_inst, const char filename[13] = "log000.txt")
	{
		fs_ = sd_inst;

		if(!file_.open(filename, O_WRITE | O_CREAT))
		{
			errorHalt("Failed to open file");
		}

		// Clear current file contents
		file_.truncate(0);

		// Flush the buffer since the file is open
		flush();
	}

	bool rename_file(const char filename[15] = "log000.txt"){
		return file_.rename(filename);
	}

	void close_file(){
		flush();
		if(!file_.close()){
			errorHalt("Failed to close file");
		}
	}

	bool open_file(const char filename[15] = "log000.txt"){
		if(!file_.open(filename, O_WRITE | O_CREAT))
		{
			return false;
		}

		// Clear current file contents
		file_.truncate(0);
		return true;
	}

	FsFile *get_file(){
		return &file_;
	}

	size_t internal_size() const noexcept override
	{
		return log_buffer_.size();
	}

	size_t ready_buffer_internal_size() const noexcept override
	{
		return ready_buffer_.size();
	}

	size_t ready_buffer_internal_capacity()
	{
		return ready_buffer_.capacity();
	}

	bool ready_buffer_exists() const noexcept override
	{
		return true;
	}


	size_t buffer_is_empty()
	{
		return log_buffer_.empty();
	}

	void prepareBuffer()
	{
		size_t val_size = sizeof(char);
		for(size_t i = 0; i < (READY_BUFFER_SIZE/val_size); i++){
			if(!ready_buffer_.full()){
				char c = log_buffer_.get();
				if(c == char()){
					break;
				}
				ready_buffer_.put(c);
			}
			else{
				break;
			}
		}
	}

  protected:
	void log_putc(char c) noexcept final
	{
		log_buffer_.put(c);
	}

	size_t internal_capacity() const noexcept override
	{
		return log_buffer_.capacity();
	}

	void flush_() noexcept final
	{
		if(!ready_buffer_.empty()){
			writeBufferToSDFile(&ready_buffer_);
		}
		else{
			writeBufferToSDFile(&log_buffer_);
		}
	}

	void clear_() noexcept final
	{
		log_buffer_.reset();
	}

	template<size_t T>
	void writeBufferToSDFile(CircularBuffer<char, T> *circular_buffer)
	{
		int bytes_written = 0;

		// We need to get the front, the rear, and potentially write the files in two steps
		// to prevent ordering problems
		size_t head = circular_buffer->head();
		size_t tail = circular_buffer->tail();
		const char* buffer = circular_buffer->storage();

		if((head < tail) || ((tail > 0) && (circular_buffer->size() == circular_buffer->capacity())))
		{
			// we have a wraparound case
			// We will write from buffer[tail] to buffer[size] in one go
			// Then we'll reset head to 0 so that we can write 0 to tail next
			bytes_written = file_.write(&buffer[tail], circular_buffer->capacity() - tail);
			bytes_written += file_.write(buffer, head);
		}
		else
		{
			// Write from tail position and send the specified number of bytes
			bytes_written = file_.write(&buffer[tail], circular_buffer->size());
		}

		if(static_cast<size_t>(bytes_written) != circular_buffer->size())
		{
			errorHalt("Failed to write to log file");
		}

		file_.flush();
		circular_buffer->reset();
	}

  private:
	SdFs* fs_;
	mutable FsFile file_;

  protected:
	CircularBuffer<char, BUFFER_SIZE> log_buffer_;
	CircularBuffer<char, READY_BUFFER_SIZE> ready_buffer_;
};

#endif // SD_FILE_LOGGER_H_
