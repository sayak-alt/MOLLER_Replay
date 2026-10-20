#pragma once
#include <map>
#include <string>
class TTree {std::map<std::string,void*> data;public:
 template<class T>void Branch(const char* key,T* ptr){data[key]=ptr;}
 template<class T>void Branch(const char* key,T* ptr,const char*){data[key]=ptr;}
 template<class T>T& Get(const char* key){return *static_cast<T*>(data.at(key));}
};
