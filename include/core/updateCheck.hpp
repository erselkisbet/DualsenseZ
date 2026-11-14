#pragma once
// Version comparison for the update notice. Framework-free so the core tests can cover it.
#include <array>
#include <cctype>
#include <string>
#include <vector>

namespace UpdateCheck {
struct Version {
    std::array<int,3> core{};
    std::vector<std::string> prerelease; // empty for a final release
    bool valid=false;
};
// Accepts "1.2.3", "v1.2.3-beta.1" and "1.2.3+build"; anything else is invalid.
inline Version Parse(std::string text) {
    Version version;
    if(!text.empty()&&(text[0]=='v'||text[0]=='V'))text.erase(0,1);
    if(const auto build=text.find('+');build!=std::string::npos)text.erase(build);
    std::string prerelease;
    if(const auto dash=text.find('-');dash!=std::string::npos){prerelease=text.substr(dash+1);text.erase(dash);if(prerelease.empty())return version;}
    size_t part=0,start=0;
    while(true) {
        const auto dot=text.find('.',start);
        const auto piece=text.substr(start,dot==std::string::npos?std::string::npos:dot-start);
        if(part>=3||piece.empty()||piece.size()>6)return version;
        for(unsigned char c:piece)if(!std::isdigit(c))return version;
        version.core[part++]=std::stoi(piece);
        if(dot==std::string::npos)break;
        start=dot+1;
    }
    for(start=0;!prerelease.empty();) {
        const auto dot=prerelease.find('.',start);
        const auto piece=prerelease.substr(start,dot==std::string::npos?std::string::npos:dot-start);
        if(piece.empty())return version;
        version.prerelease.push_back(piece);
        if(dot==std::string::npos)break;
        start=dot+1;
    }
    version.valid=true;
    return version;
}
// Semantic-version ordering: <0 when a is older than b, 0 when equal, >0 when newer.
inline int Compare(const Version& a,const Version& b) {
    for(int i=0;i<3;++i)if(a.core[i]!=b.core[i])return a.core[i]<b.core[i]?-1:1;
    if(a.prerelease.empty()!=b.prerelease.empty())return a.prerelease.empty()?1:-1; // a final release outranks its pre-releases
    const auto numeric=[](const std::string& s){for(unsigned char c:s)if(!std::isdigit(c))return false;return s.size()<=9;};
    for(size_t i=0;i<a.prerelease.size()&&i<b.prerelease.size();++i) {
        const auto& x=a.prerelease[i];const auto& y=b.prerelease[i];
        const bool nx=numeric(x),ny=numeric(y);
        if(nx&&ny){const long vx=std::stol(x),vy=std::stol(y);if(vx!=vy)return vx<vy?-1:1;}
        else if(nx!=ny)return nx?-1:1; // numbers sort before words
        else if(x!=y)return x<y?-1:1;
    }
    if(a.prerelease.size()!=b.prerelease.size())return a.prerelease.size()<b.prerelease.size()?-1:1;
    return 0;
}
inline bool IsNewer(const std::string& candidate,const std::string& current) {
    const auto a=Parse(candidate),b=Parse(current);
    return a.valid&&b.valid&&Compare(a,b)>0;
}
// Whether a published version should be offered to someone running `current`:
// it must be newer, and pre-releases are offered only to people already on a pre-release.
inline bool Offered(const std::string& candidate,const std::string& current) {
    return IsNewer(candidate,current)&&(Parse(candidate).prerelease.empty()||!Parse(current).prerelease.empty());
}
}
