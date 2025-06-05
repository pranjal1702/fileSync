#include<string>
struct DeltaInstruction {
    enum class Type { COPY, INSERT };
    Type type;

    size_t offset = 0;
    size_t length = 0;
    std::string data;

    DeltaInstruction(Type t, size_t o, size_t l)
        : type(t), offset(o), length(l) {}

    DeltaInstruction(Type t, const std::string& d)
        : type(t), offset(0), length(d.size()), data(d) {}
};
