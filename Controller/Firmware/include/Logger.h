#include <Print.h>

class Logger : public Print {

public:
    Logger() = default;
    virtual ~Logger() = default;
    size_t write(uint8_t) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    int availableForWrite() override;
    void flush() override;
};