#include <atomic>
#include <memory>

struct Type {

};

int main() 
{
    std::atomic<std::shared_ptr<Type>> test = std::make_shared<Type>();
    return 0;
}