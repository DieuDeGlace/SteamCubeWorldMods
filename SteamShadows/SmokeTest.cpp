#include "SteamShadows.cpp"
int main(){ID3D11Device* d=nullptr;ID3D11DeviceContext* c=nullptr;HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,nullptr,&c);if(FAILED(hr))return 1;bool ok=shadows::initialize(c);printf("Shadow shaders, resources and context-state support: %s\n",ok?"PASS":"FAIL");return ok?0:2;}
