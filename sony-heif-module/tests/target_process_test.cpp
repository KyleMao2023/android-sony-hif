#include "target_process.h"
#include <cstdio>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %s at %d\n",#x,__LINE__);return 1;}}while(0)
int main(){
    CHECK(isTargetProcess("com.miui.gallery"));
    CHECK(isTargetProcess("com.tencent.mm"));
    CHECK(isTargetProcess("jp.co.sony.ips.portalapp"));
    CHECK(!isTargetProcess(nullptr));
    CHECK(!isTargetProcess(""));
    CHECK(!isTargetProcess("jp.co.sony.ips.portalapp:remote"));
    CHECK(!isTargetProcess("com.tencent.mm:tools"));
    CHECK(!isTargetProcess("com.miui.gallery.extra"));
    CHECK(!isTargetProcess("com.android.systemui"));
    std::puts("exact target main-process tests passed");
}
