#include "COWString.hpp"
#include <iostream>
int main()
{
    COWString s("xx");
    std::cout << s.UseCount() << std::endl;
    COWString s1 = s;
    std::cout << s1.UseCount() << std::endl;
    COWString s2("sadasd");
    s = s2;
    s2 = s2;
    std::cout << s2.UseCount() << std::endl;
    std::cout << s.UseCount() << std::endl;
    s[1] = 'z';
    std::cout << s.c_str() << std::endl;
    std::cout << s2.UseCount() << std::endl;
    std::cout << s.UseCount() << std::endl;
    std::cout << s2.c_str() << std::endl;
    std::cout << s2[1] << std::endl;
    std::cout << s2.UseCount() << std::endl;
    const COWString s3 = s2;
    std::cout << s3[2] << std::endl;
    std::cout << s2.UseCount() << std::endl;
    for (auto it = s2.cbegin(); it != s2.end() /*compare with non const iterator*/; ++it) {

        std::cout << "const -- " << *it << std::endl;
    }
    for (const auto &ch : s2) {
        // ch = 'a';
        std::cout << "for range -- " << ch << std::endl;
    }
    std::cout << s2.UseCount() << std::endl;
    std::cout << s2 << std::endl;
    std::cout << s2 + "qqq" << std::endl;
    return 0;
}