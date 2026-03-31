#pragma once
#include "resource.h"

class CNetPortChkApp : public CWinApp
{
public:
    CNetPortChkApp();

    BOOL InitInstance() override;
    int  ExitInstance() override;

    DECLARE_MESSAGE_MAP()

private:
    bool        m_wsaInit       { false };
};

extern CNetPortChkApp theApp;
