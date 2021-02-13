
// WebBrowserDlg.cpp : implementation file
//

#include "stdafx.h"
#include "framework.h"
#include "WebBrowser.h"
#include "WebBrowserDlg.h"
#include "afxdialogex.h"
#include "winuser.h"
#include "ViewComponent.h"
#include <Shellapi.h>
#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CWebBrowserDlg dialog

static constexpr UINT s_runAsyncWindowMessage = WM_APP;

// CEdgeBrowserAppDlg dialog

CWebBrowserDlg::CWebBrowserDlg(CWnd* pParent /*=nullptr*/)
    : CDialog(IDD_WEBROWSER_DIALOG, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    g_hInstance = GetModuleHandle(NULL);

}
void CWebBrowserDlg::DoDataExchange(CDataExchange* pDX)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CDialog::DoDataExchange(pDX);
   DDX_Control(pDX, IDC_EDGE,(CWnd&)m_webView);
}

BEGIN_MESSAGE_MAP(CWebBrowserDlg, CDialog)
    ON_WM_SYSCOMMAND()
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_WM_SIZE()
    ON_WM_DRAWITEM()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_NCHITTEST()
    ON_WM_CLOSE()
END_MESSAGE_MAP()

BEGIN_EVENTSINK_MAP(CWebBrowserDlg,CDialog)
ON_EVENT(CWebBrowserDlg, IDC_EDGE, 1/* ClickIn */, CWebBrowserDlg::WebView2Explorer, VTS_DISPATCH VTS_PVARIANT)
END_EVENTSINK_MAP()

// CWebBrowserDlg message handlers

BOOL CWebBrowserDlg::OnInitDialog()
{
     CDialog::OnInitDialog();

    HRESULT hresult = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    SetWindowLongPtr(this->GetSafeHwnd(), GWLP_USERDATA, (LONG_PTR)this);

    CMenu* pSysMenu = GetSystemMenu(FALSE);
    if (pSysMenu != nullptr)
    {
        BOOL bNameValid;
        CString strAboutMenu;
        bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
        ASSERT(bNameValid);
        if (!strAboutMenu.IsEmpty())
        {
            pSysMenu->AppendMenu(MF_SEPARATOR);
            pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
        }
    }

    // Set the icon for this dialog.  The framework does this automatically
    //  when the application's main window is not a dialog
    SetIcon(m_hIcon, TRUE);			// Set big icon
    SetIcon(m_hIcon, FALSE);		// Set small icon

    // TODO: Add extra initialization here
    InitializeWebView();

    return TRUE;  // return TRUE  unless you set the focus to a control
}


void CWebBrowserDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    if(nID == SC_CLOSE)
    {
        CDialog::OnOK();
    }
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CWebBrowserDlg::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this); // device context for painting

        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

        // Center icon in client rectangle
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;

        // Draw the icon
        dc.DrawIcon(x, y, m_hIcon);
    }
    else
    {
        CDialog::OnPaint();
    }
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CWebBrowserDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}
// Register the Win32 window class for the app window.

void CWebBrowserDlg::ResizeEverything()
{
    RECT availableBounds = { 0 };
    GetClientRect(&availableBounds);
    // ClientToScreen(&availableBounds);

    if (auto view = GetComponent<ViewComponent>())
    {
        view->SetBounds(availableBounds);
    }
}

void CWebBrowserDlg::RunAsync(std::function<void()> callback)
{
    auto* task = new std::function<void()>(callback);
    PostMessage(s_runAsyncWindowMessage, reinterpret_cast<WPARAM>(task), 0);
}

void CWebBrowserDlg::InitializeWebView()
{

    CloseWebView();
    m_dcompDevice = nullptr;


    HRESULT hr2 = DCompositionCreateDevice2(nullptr, IID_PPV_ARGS(&m_dcompDevice));
    if (!SUCCEEDED(hr2))
    {
        AfxMessageBox(L"Attempting to create WebView using DComp Visual is not supported.\r\n"
            "DComp device creation failed.\r\n"
            "Current OS may not support DComp.\r\n"
            "Create with Windowless DComp Visual Failed", MB_OK);
        return;
    }


#ifdef USE_WEBVIEW2_WIN10
    m_wincompCompositor = nullptr;
#endif
    LPCWSTR subFolder = nullptr;
    auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    options->put_AllowSingleSignOnUsingOSPrimaryAccount(FALSE);


    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(subFolder, nullptr, options.Get(), Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(this, &CWebBrowserDlg::OnCreateEnvironmentCompleted).Get());
    if (!SUCCEEDED(hr))
    {
        if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
        {
            TRACE("Couldn't find Edge installation. Do you have a version installed that is compatible with this ");
        }
        else
        {
            AfxMessageBox(L"Failed to create webview environment");
        }
    }
}

HRESULT CWebBrowserDlg::DCompositionCreateDevice2(IUnknown* renderingDevice, REFIID riid, void** ppv)
{
    HRESULT hr = E_FAIL;
    static decltype(::DCompositionCreateDevice2)* fnCreateDCompDevice2 = nullptr;
    if (fnCreateDCompDevice2 == nullptr)
    {
        HMODULE hmod = ::LoadLibraryEx(L"dcomp.dll", nullptr, 0);
        if (hmod != nullptr)
        {
            fnCreateDCompDevice2 = reinterpret_cast<decltype(::DCompositionCreateDevice2)*>(
                ::GetProcAddress(hmod, "DCompositionCreateDevice2"));
        }
    }
    if (fnCreateDCompDevice2 != nullptr)
    {
        hr = fnCreateDCompDevice2(renderingDevice, riid, ppv);
    }
    return hr;
}

void CWebBrowserDlg::OnSize(UINT a, int b, int c)
{
}

void CWebBrowserDlg::CloseWebView(bool cleanupUserDataFolder)
{

    if (m_controller)
    {
        m_controller->Close();
        m_controller = nullptr;
        m_webView = nullptr;
    }
    m_webViewEnvironment = nullptr;
    if (cleanupUserDataFolder)
    {
        //Clean user data        
    }
}

HRESULT CWebBrowserDlg::OnCreateEnvironmentCompleted(HRESULT result, ICoreWebView2Environment* environment)
{
    m_webViewEnvironment = environment;
    m_webViewEnvironment->CreateCoreWebView2Controller(this->GetSafeHwnd(), Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(this, &CWebBrowserDlg::OnCreateCoreWebView2ControllerCompleted).Get());

    return S_OK;
}

HRESULT CWebBrowserDlg::OnCreateCoreWebView2ControllerCompleted(HRESULT result, ICoreWebView2Controller* controller)
{
    if (result == S_OK)
    {
        m_controller = controller;
        wil::com_ptr<ICoreWebView2> coreWebView2;
        m_controller->get_CoreWebView2(&coreWebView2);
        coreWebView2.query_to(&m_webView);
        NewComponent<ViewComponent>(
            this, m_dcompDevice.get(),
#ifdef USE_WEBVIEW2_WIN10
            m_wincompCompositor,
#endif
            m_creationModeId == IDM_CREATION_MODE_TARGET_DCOMP);
        HRESULT hresult = m_webView->Navigate(L"https://google.com");

        if (hresult == S_OK)
        {
            TRACE("Web Page Opened Successfully");
            ResizeEverything();
           // m_webView->add_WebMessageReceived(Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(&CWebBrowserDlg::WebView2MessageProcess).Get(),&token);
        
        }
    }
    else
    {
        TRACE("Failed to create webview");
    }
    return S_OK;
}


void CWebBrowserDlg::WebView2Explorer(LPDISPATCH pDisp, VARIANT* URL)
{
    if (URL->bstrVal)
    {
        std::wstring wsAccessToken = L"";
        std::wstring wsResponse(URL->bstrVal);
        if (-1 != wsResponse.find(L"javascript:false") || -1 != wsResponse.find(L"error=access_denied"))
        {
            CDialog::OnCancel();
        }
        if (!wsResponse.empty())
        {
            int nPos = wsResponse.find(L"access_token=");
            if (-1 != nPos)
            {
                int nPos1 = -1;
                nPos1 = wsResponse.find(L"&", nPos);
                if (-1 != nPos1)
                {
                    wsAccessToken = wsResponse.substr(nPos, nPos1 - nPos);
                }
            }
            if (!wsAccessToken.empty())
                CDialog::OnOK();
        }
    }
}

HRESULT CWebBrowserDlg::WebView2MessageHandler(PWSTR *s_Message)
{

    std::wstring wsAccessToken = L"";
    std::wstring wsResponse(*s_Message);
    if (-1 != wsResponse.find(L"javascript:false") || -1 != wsResponse.find(L"error=access_denied"))
    {
        CDialog::OnCancel();
    }
    if (!wsResponse.empty())
    {
        int nPos = wsResponse.find(L"access_token=");
        if (-1 != nPos)
        {
            int nPos1 = -1;
            nPos1 = wsResponse.find(L"&", nPos);
            if (-1 != nPos1)
            {
                wsAccessToken = wsResponse.substr(nPos, nPos1 - nPos);
            }
        }
        if (!wsAccessToken.empty())
            CDialog::OnOK();

    }

    return S_OK;
}