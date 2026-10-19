#pragma once
#include <cstring>
inline bool isTargetProcess(const char* name){
    return name&&(!std::strcmp(name,"com.miui.gallery")||!std::strcmp(name,"com.tencent.mm")||
                 !std::strcmp(name,"jp.co.sony.ips.portalapp"));
}
