#pragma once
#include <string>
#include <unordered_map>
namespace UiText {
using Dictionary=std::unordered_map<std::string,std::string>;
inline std::string Resolve(Dictionary const& dictionary,std::string const& text) {
    if(auto found=dictionary.find(text);found!=dictionary.end())return found->second;
    std::string result;size_t from=0;
    while(from<text.size()) {
        auto start=text.find('@',from);if(start==std::string::npos){result+=text.substr(from);break;}
        result+=text.substr(from,start-from);auto end=text.find('@',start+1);
        if(end==std::string::npos){result+=text.substr(start);break;}
        auto token=text.substr(start,end-start+1);auto found=dictionary.find(token);
        result+=found==dictionary.end()?token:found->second;from=end+1;
    }return result;
}
}
