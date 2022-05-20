
// AutoLikeDlg.cpp: 實作檔案
//

#include "pch.h"
#include "framework.h"
#include "AutoLike.h"
#include "AutoLikeDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CAutoLikeDlg* g_pDlg;
HHOOK _hMouseHook;
HHOOK _hKeyboardHook;
BOOL _bEnable;
POINT _pCurrnetMouse;
POINT _pStartMouse;
POINT _pNextMouse;
bool g_bProcessing = false;

LRESULT CALLBACK MouseHookProc (int nCode, WPARAM wParam, LPARAM lParam) 
{
	if (wParam == WM_MOUSEMOVE || wParam == WM_LBUTTONDOWN) {
		GetCursorPos (&_pCurrnetMouse);
		CString csTemp;
		csTemp.Format (L"Current Mouse: %d, %d", _pCurrnetMouse.x, _pCurrnetMouse.y);
		::SetWindowText (::GetDlgItem (g_pDlg->m_hWnd, IDC_STATIC_MOUSE_POS), csTemp.GetString ());
	}
	return CallNextHookEx (_hMouseHook, nCode, wParam, lParam);
}
bool GetExeDir (_TCHAR* psz)
{
	if (!psz)
		return false;

	_TCHAR sz[MAX_PATH] = _T ("");
	GetModuleFileName (NULL, sz, MAX_PATH);

	_TCHAR szDrv[_MAX_DRIVE] = _T ("");
	_TCHAR szDir[_MAX_DIR] = _T ("");
	_TCHAR szName[_MAX_FNAME] = _T ("");
	_TCHAR szExt[_MAX_EXT] = _T ("");
	_tsplitpath_s (sz, szDrv, _MAX_DRIVE, szDir, _MAX_DIR, szName, _MAX_FNAME, szExt, _MAX_EXT);

	_stprintf_s (psz, MAX_PATH, _T ("%s%s"), szDrv, szDir);

	return true;
}
#define VISION_TOLERANCE 0.0000001
#define D2R (CV_PI / 180.0)
#define R2D (180.0 / CV_PI)
bool comparePtWithAngle (const pair<Point2f, double> lhs, const pair<Point2f, double> rhs) { return lhs.second < rhs.second; }
bool compareScoreBig2Small (const s_MatchParameter& lhs, const s_MatchParameter& rhs) { return  lhs.dMatchScore > rhs.dMatchScore; }
bool compareMatchResultByPosY (const s_MatchParameter& lhs, const s_MatchParameter& rhs) { return lhs.pt.y < rhs.pt.y; }

// 對 App About 使用 CAboutDlg 對話方塊

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 對話方塊資料
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支援

// 程式碼實作
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CAutoLikeDlg 對話方塊



CAutoLikeDlg::CAutoLikeDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_AUTOLIKE_DIALOG, pParent)
	, m_iOffsetX (143)
	, m_iOffsetY (-56)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CAutoLikeDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange (pDX);
	DDX_Text (pDX, IDC_EDIT_OFFSETX, m_iOffsetX);
	DDX_Text (pDX, IDC_EDIT_OFFSETY, m_iOffsetY);
	DDX_Control (pDX, IDC_CHECK_SHOWDEBUG, m_ckDebug);
	DDX_Control (pDX, IDC_CHECK_CLICK, m_ckClick);
}

BEGIN_MESSAGE_MAP(CAutoLikeDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_MOUSEMOVE ()
	ON_BN_CLICKED (IDC_BUTTON_EXECUTE, &CAutoLikeDlg::OnBnClickedButtonExecute)
END_MESSAGE_MAP()


// CAutoLikeDlg 訊息處理常式

BOOL CAutoLikeDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 將 [關於...] 功能表加入系統功能表。

	// IDM_ABOUTBOX 必須在系統命令範圍之中。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

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

	// 設定此對話方塊的圖示。當應用程式的主視窗不是對話方塊時，
	// 框架會自動從事此作業
	SetIcon(m_hIcon, TRUE);			// 設定大圖示
	SetIcon(m_hIcon, FALSE);		// 設定小圖示

	// TODO: 在此加入額外的初始設定
	g_pDlg = this;
	HMODULE hInstance = GetModuleHandle (NULL);
	_hMouseHook = SetWindowsHookEx (WH_MOUSE_LL, MouseHookProc, hInstance, 0);

	TCHAR szRet[MAX_PATH] = _T (".");
	GetExeDir (szRet);
	CString strEmojiPath;
	strEmojiPath.Format (L"%s\\Emoji.jpg", szRet);
	USES_CONVERSION;
	m_matEmogi = imread (String (T2A (strEmojiPath)), IMREAD_GRAYSCALE);
	m_ckClick.SetCheck (TRUE);
	LearnPattern (m_matEmogi);
	return TRUE;  // 傳回 TRUE，除非您對控制項設定焦點
}

void CAutoLikeDlg::OnBnClickedButtonExecute ()
{
	Mat matScreen = GetScreenShot ();
	int iW = matScreen.cols;
	int iH = matScreen.rows;
	Rect rect (Point (iW / 4, 100), Point (iW / 4 * 3, iH - 100));
	matScreen = matScreen (rect);
	Mat mat;
	cvtColor (matScreen, mat, COLOR_BGR2GRAY);
	if (mat.empty () || m_matEmogi.empty ())
	{
		AfxMessageBox (L"沒有Emoji");
		g_bProcessing = false;
		return;
	}
	Match (mat, m_matEmogi, rect);
	//matchTemplate (mat)
}
void CAutoLikeDlg::LeftClick ()
{
	INPUT    Input = { 0 };
	//Under the left?
	Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
	::SendInput (1, &Input, sizeof (INPUT));
	//Left-click to lift
	::ZeroMemory (&Input, sizeof (INPUT));
	Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
	::SendInput (1, &Input, sizeof (INPUT));
}

void CAutoLikeDlg::RightClick ()
{
	INPUT    Input = { 0 };
	//Right click to press the
	Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
	::SendInput (1, &Input, sizeof (INPUT));
	//Right-click to lift
	::ZeroMemory (&Input, sizeof (INPUT));
	Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
	::SendInput (1, &Input, sizeof (INPUT));
}
void CAutoLikeDlg::MouseMove (int x, int y)
{
	double fScreenWidth = ::GetSystemMetrics (SM_CXSCREEN) - 1;
	double fScreenHeight = ::GetSystemMetrics (SM_CYSCREEN) - 1;
	double fx = x * (65535.0f / fScreenWidth);
	double fy = y * (65535.0f / fScreenHeight);
	INPUT  Input = { 0 };
	Input.type = INPUT_MOUSE;
	Input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
	Input.mi.dx = fx;
	Input.mi.dy = fy;
	::SendInput (1, &Input, sizeof (INPUT));
}
double m_dScore = 0.8;
BOOL CAutoLikeDlg::Match (Mat matSrc, Mat matDst, Rect rectRoi)
{
	//決定金字塔層數 總共為1 + iLayer層
	int iTopLayer = GetTopLayer (&matDst, (int)sqrt ((double)1024));
	//建立金字塔
	vector<Mat> vecMatSrcPyr;
		buildPyramid (matSrc, vecMatSrcPyr, iTopLayer);

	s_TemplData* pTemplData = &m_TemplData;

	//第一階段以最頂層找出大致角度與ROI
	double dAngleStep = atan (2.0 / max (pTemplData->vecPyramid[iTopLayer].cols, pTemplData->vecPyramid[iTopLayer].rows)) * R2D;

	vector<double> vecAngles;
	vecAngles.push_back (0.0);

	int iTopSrcW = vecMatSrcPyr[iTopLayer].cols, iTopSrcH = vecMatSrcPyr[iTopLayer].rows;
	Point2f ptCenter ((iTopSrcW - 1) / 2.0f, (iTopSrcH - 1) / 2.0f);
	int iMatchCount = 15;

	int iSize = (int)vecAngles.size ();
	vector<s_MatchParameter> vecMatchParameter (iSize * (iMatchCount));
	for (int i = 0; i < iSize; i++)
	{
		Mat matRotatedSrc, matR = getRotationMatrix2D (ptCenter, vecAngles[i], 1);
		Mat matResult;
		Point ptMaxLoc;
		double dValue, dMaxVal;
		double dRotate = clock ();
		Size sizeBest = GetBestRotationSize (vecMatSrcPyr[iTopLayer].size (), pTemplData->vecPyramid[iTopLayer].size (), vecAngles[i]);
		float fTranslationX = (sizeBest.width - 1) / 2.0f - ptCenter.x;
		float fTranslationY = (sizeBest.height - 1) / 2.0f - ptCenter.y;
		matR.at<double> (0, 2) += fTranslationX;
		matR.at<double> (1, 2) += fTranslationY;
		warpAffine (vecMatSrcPyr[iTopLayer], matRotatedSrc, matR, sizeBest);


		MatchTemplate (matRotatedSrc, pTemplData, matResult, iTopLayer);

		minMaxLoc (matResult, 0, &dMaxVal, 0, &ptMaxLoc);

		vecMatchParameter[i * (iMatchCount)] = s_MatchParameter (Point2f (ptMaxLoc.x - fTranslationX, ptMaxLoc.y - fTranslationY), dMaxVal, vecAngles[i]);

		for (int j = 0; j < iMatchCount - 1; j++)
		{
			ptMaxLoc = GetNextMaxLoc (matResult, ptMaxLoc, -1, pTemplData->vecPyramid[iTopLayer].cols, pTemplData->vecPyramid[iTopLayer].rows, dValue, 0);
			vecMatchParameter[i * (iMatchCount) + j + 1] = s_MatchParameter (Point2f (ptMaxLoc.x - fTranslationX, ptMaxLoc.y - fTranslationY), dValue, vecAngles[i]);
		}
	}
	FilterWithScore (&vecMatchParameter, m_dScore - 0.05*iTopLayer);

	//紀錄旋轉矩形、ROI與角度
	int iMatchSize = (int)vecMatchParameter.size ();
	int iDstW = pTemplData->vecPyramid[iTopLayer].cols, iDstH = pTemplData->vecPyramid[iTopLayer].rows;
	//if (m_bDebugMode)
	{
		for (int i = 0; i < iMatchSize; i++)
		{
			Point2f ptLT, ptRT, ptRB, ptLB;
			double dRAngle = -vecMatchParameter[i].dMatchAngle * D2R;
			ptLT = ptRotatePt2f (vecMatchParameter[i].pt, ptCenter, dRAngle);
			ptRT = Point2f (ptLT.x + iDstW * (float)cos (dRAngle), ptLT.y - iDstW * (float)sin (dRAngle));
			ptLB = Point2f (ptLT.x + iDstH * (float)sin (dRAngle), ptLT.y + iDstH * (float)cos (dRAngle));
			ptRB = Point2f (ptRT.x + iDstH * (float)sin (dRAngle), ptRT.y + iDstH * (float)cos (dRAngle));
			//紀錄旋轉矩形
			Point2f ptRectCenter = Point2f ((ptLT.x + ptRT.x + ptLB.x + ptRB.x) / 4.0f, (ptLT.y + ptRT.y + ptLB.y + ptRB.y) / 4.0f);
			vecMatchParameter[i].rectR = RotatedRect (ptRectCenter, pTemplData->vecPyramid[iTopLayer].size (), (float)vecMatchParameter[i].dMatchAngle);

		}
		//紀錄旋轉矩形
	}

	//FilterWithRotatedRect (&vecMatchParameter, CV_TM_CCORR_NORMED, m_dMaxOverlap);

	//顯示第一層結果
	/*Mat matShow1;

	cvtColor (vecMatSrcPyr[iTopLayer], matShow1, CV_GRAY2BGR);
	iMatchSize = (int)vecMatchParameter.size ();
	string str1 = format ("Toplayer", iMatchSize);

	for (int i = 0; i < iMatchSize; i++)
	{
		Point2f ptLT, ptRT, ptRB, ptLB;
		double dRAngle = -vecMatchParameter[i].dMatchAngle * D2R;
		ptLT = ptRotatePt2f (vecMatchParameter[i].pt, ptCenter, dRAngle);
		ptRT = Point2f (ptLT.x + iDstW * (float)cos (dRAngle), ptLT.y - iDstW * (float)sin (dRAngle));
		ptLB = Point2f (ptLT.x + iDstH * (float)sin (dRAngle), ptLT.y + iDstH * (float)cos (dRAngle));
		ptRB = Point2f (ptRT.x + iDstH * (float)sin (dRAngle), ptRT.y + iDstH * (float)cos (dRAngle));
		line (matShow1, ptLT, ptLB, Scalar (0, 255, 0));
		line (matShow1, ptLB, ptRB, Scalar (0, 255, 0));
		line (matShow1, ptRB, ptRT, Scalar (0, 255, 0));
		line (matShow1, ptRT, ptLT, Scalar (0, 255, 0));
		circle (matShow1, ptLT, 1, Scalar (0, 0, 255));

		string strText = format ("%d", i);
		putText (matShow1, strText, ptLT, FONT_HERSHEY_PLAIN, 1, Scalar (0, 255, 0));
		imshow (str1, matShow1);

	}
	imshow (str1, matShow1);
	moveWindow (str1, 0, 0);*/

	//第一階段結束
	int iStopLayer = 0;
	//int iSearchSize = min (m_iMaxPos + MATCH_CANDIDATE_NUM, (int)vecMatchParameter.size ());//可能不需要搜尋到全部 太浪費時間
	vector<s_MatchParameter> vecAllResult;
	for (int i = 0; i < (int)vecMatchParameter.size (); i++)
		//for (int i = 0; i < iSearchSize; i++)
	{
		double dRAngle = -vecMatchParameter[i].dMatchAngle * D2R;
		Point2f ptLT = ptRotatePt2f (vecMatchParameter[i].pt, ptCenter, dRAngle);

		double dAngleStep = atan (2.0 / max (iDstW, iDstH)) * R2D;//min改為max
		vecMatchParameter[i].dAngleStart = vecMatchParameter[i].dMatchAngle - dAngleStep;
		vecMatchParameter[i].dAngleEnd = vecMatchParameter[i].dMatchAngle + dAngleStep;

		if (iTopLayer <= iStopLayer)
		{
			vecMatchParameter[i].pt = Point2d (ptLT * ((iTopLayer == 0) ? 1 : 2));
			vecAllResult.push_back (vecMatchParameter[i]);
		}
		else
		{
			for (int iLayer = iTopLayer - 1; iLayer >= iStopLayer; iLayer--)
			{
				//搜尋角度
				dAngleStep = atan (2.0 / max (pTemplData->vecPyramid[iLayer].cols, pTemplData->vecPyramid[iLayer].rows)) * R2D;//min改為max
				vector<double> vecAngles;
				double dAngleS = vecMatchParameter[i].dAngleStart, dAngleE = vecMatchParameter[i].dAngleEnd;
					vecAngles.push_back (0.0);
				Point2f ptSrcCenter ((vecMatSrcPyr[iLayer].cols - 1) / 2.0f, (vecMatSrcPyr[iLayer].rows - 1) / 2.0f);
				iSize = (int)vecAngles.size ();
				vector<s_MatchParameter> vecNewMatchParameter (iSize);
				int iMaxScoreIndex = 0;
				double dBigValue = -1;
				for (int j = 0; j < iSize; j++)
				{
					Mat rMat, matResult, matRotatedSrc;
					double dMaxValue = 0;
					Point ptMaxLoc;
					if (iLayer == 0)
						int a = 0;
					GetRotatedROI (vecMatSrcPyr[iLayer], pTemplData->vecPyramid[iLayer].size (), ptLT * 2, vecAngles[j], matRotatedSrc);

					MatchTemplate (matRotatedSrc, pTemplData, matResult, iLayer);
					minMaxLoc (matResult, 0, &dMaxValue, 0, &ptMaxLoc);
					vecNewMatchParameter[j] = s_MatchParameter (ptMaxLoc, dMaxValue, vecAngles[j]);

					if (vecNewMatchParameter[j].dMatchScore > dBigValue)
					{
						iMaxScoreIndex = j;
						dBigValue = vecNewMatchParameter[j].dMatchScore;
					}
					//次像素估計
					if (ptMaxLoc.x == 0 || ptMaxLoc.y == 0 || ptMaxLoc.y == matResult.cols - 1 || ptMaxLoc.x == matResult.rows - 1)
						vecNewMatchParameter[j].bPosOnBorder = TRUE;
					if (!vecNewMatchParameter[j].bPosOnBorder)
					{
						for (int y = -1; y <= 1; y++)
							for (int x = -1; x <= 1; x++)
								vecNewMatchParameter[j].vecResult[x + 1][y + 1] = matResult.at<float> (ptMaxLoc + Point (x, y));
					}
					//次像素估計
				}
				//sort (vecNewMatchParameter.begin (), vecNewMatchParameter.end (), compareScoreBig2Small);
				if (vecNewMatchParameter[iMaxScoreIndex].dMatchScore < m_dScore - 0.05 * iLayer)
					break;


				double dNewMatchAngle = vecNewMatchParameter[iMaxScoreIndex].dMatchAngle;

				//讓坐標系回到旋轉時(GetRotatedROI)的(0, 0)
				Point2f ptPaddingLT = ptRotatePt2f (ptLT * 2, ptSrcCenter, dNewMatchAngle * D2R) - Point2f (3, 3);
				Point2f pt (vecNewMatchParameter[iMaxScoreIndex].pt.x + ptPaddingLT.x, vecNewMatchParameter[iMaxScoreIndex].pt.y + ptPaddingLT.y);
				//再旋轉
				pt = ptRotatePt2f (pt, ptSrcCenter, -dNewMatchAngle * D2R);

				if (iLayer == iStopLayer)
				{
					vecNewMatchParameter[iMaxScoreIndex].pt = pt * (iStopLayer == 0 ? 1 : 2);
					vecAllResult.push_back (vecNewMatchParameter[iMaxScoreIndex]);
				}
				else
				{
					//更新MatchAngle ptLT
					vecMatchParameter[i].dMatchAngle = dNewMatchAngle;
					vecMatchParameter[i].dAngleStart = vecMatchParameter[i].dMatchAngle - dAngleStep / 2;
					vecMatchParameter[i].dAngleEnd = vecMatchParameter[i].dMatchAngle + dAngleStep / 2;
					ptLT = pt;
				}
			}
		}
	}
	FilterWithScore (&vecAllResult, m_dScore);
	sort (vecAllResult.begin (), vecAllResult.end (), compareMatchResultByPosY);
	//
	
	//最後再次濾掉重疊
	iDstW = pTemplData->vecPyramid[iStopLayer].cols, iDstH = pTemplData->vecPyramid[iStopLayer].rows;

	for (int i = 0; i < (int)vecAllResult.size (); i++)
	{
		Point2f ptLT, ptRT, ptRB, ptLB;
		double dRAngle = -vecAllResult[i].dMatchAngle * D2R;
		ptLT = vecAllResult[i].pt;
		ptRT = Point2f (ptLT.x + iDstW * (float)cos (dRAngle), ptLT.y - iDstW * (float)sin (dRAngle));
		ptLB = Point2f (ptLT.x + iDstH * (float)sin (dRAngle), ptLT.y + iDstH * (float)cos (dRAngle));
		ptRB = Point2f (ptRT.x + iDstH * (float)sin (dRAngle), ptRT.y + iDstH * (float)cos (dRAngle));
		//紀錄旋轉矩形
		Point2f ptRectCenter = Point2f ((ptLT.x + ptRT.x + ptLB.x + ptRB.x) / 4.0f, (ptLT.y + ptRT.y + ptLB.y + ptRB.y) / 4.0f);
		vecAllResult[i].rectR = RotatedRect (ptRectCenter, pTemplData->vecPyramid[iStopLayer].size (), (float)vecAllResult[i].dMatchAngle);
	}
	FilterWithRotatedRect (&vecAllResult, CV_TM_CCOEFF_NORMED, 0);
	//最後再次濾掉重疊
	if (vecAllResult.size () == 0)
		return FALSE;
	iMatchSize = (int)vecAllResult.size ();
	int iW = pTemplData->vecPyramid[0].cols, iH = pTemplData->vecPyramid[0].rows;
	CClientDC dc (NULL);
	UpdateData (TRUE);
	for (int i = 0; i < iMatchSize; i++)
	{
		if (!m_ckClick.GetCheck ())
		break;
		MouseMove (vecAllResult[i].pt.x + iW / 2 + rectRoi.x, vecAllResult[i].pt.y + iH / 2 + rectRoi.y);
		Sleep (50);

		LeftClick ();
		Sleep (50);
		MouseMove (vecAllResult[i].pt.x + iW / 2 + m_iOffsetX + rectRoi.x, vecAllResult[i].pt.y + iH / 2 + m_iOffsetY + rectRoi.y);
		Sleep (50);

		LeftClick ();
		Sleep (50);
	}
	CRect rect1;
	GetDlgItem (IDC_BUTTON_EXECUTE)->GetWindowRect (rect1);
	MouseMove (rect1.left + rect1.Width () / 2, rect1.top + rect1.Height () / 2);

	if (m_ckDebug.GetCheck ())
	{
		Mat matShow;

		cvtColor (vecMatSrcPyr[0], matShow, CV_GRAY2BGR);
		iMatchSize = (int)vecAllResult.size ();
		string str = format ("Toplayer, Candidate:%d", iMatchSize);
		iDstW = matDst.cols;
		iDstH = matDst.rows;
		for (int i = 0; i < iMatchSize; i++)
		{
			Point2f ptLT, ptRT, ptRB, ptLB;
			double dRAngle = -vecAllResult[i].dMatchAngle * D2R;
			ptLT = ptRotatePt2f (vecAllResult[i].pt, ptCenter, dRAngle);
			ptRT = Point2f (ptLT.x + iDstW * (float)cos (dRAngle), ptLT.y - iDstW * (float)sin (dRAngle));
			ptLB = Point2f (ptLT.x + iDstH * (float)sin (dRAngle), ptLT.y + iDstH * (float)cos (dRAngle));
			ptRB = Point2f (ptRT.x + iDstH * (float)sin (dRAngle), ptRT.y + iDstH * (float)cos (dRAngle));
			line (matShow, ptLT, ptLB, Scalar (0, 255, 0));
			line (matShow, ptLB, ptRB, Scalar (0, 255, 0));
			line (matShow, ptRB, ptRT, Scalar (0, 255, 0));
			line (matShow, ptRT, ptLT, Scalar (0, 255, 0));
			circle (matShow, ptLT, 1, Scalar (0, 0, 255));
			vecAllResult[i].rectRoi = Rect (ptLT, ptRB);
			string strText = format ("%d", i);
			putText (matShow, strText, ptLT, FONT_HERSHEY_PLAIN, 1, Scalar (0, 255, 0));
			imshow (str, matShow);

		}
		resize (matShow, matShow, matShow.size () / 3 * 2);
		imshow (str, matShow);
		moveWindow (str, 0, 0);
	}

	//
	return TRUE;
}
int CAutoLikeDlg::GetTopLayer (Mat* matTempl, int iMinDstLength)
{
	int iTopLayer = 0;
	int iMinReduceArea = iMinDstLength * iMinDstLength;
	int iArea = matTempl->cols * matTempl->rows;
	while (iArea > iMinReduceArea)
	{
		iArea /= 4;
		iTopLayer++;
	}
	return iTopLayer;
}
Point CAutoLikeDlg::GetNextMaxLoc (Mat & matResult, Point ptMaxLoc, double dMinValue, int iTemplateW, int iTemplateH, double& dMaxValue, double dMaxOverlap)
{
	//比對到的區域完全不重疊 : +-一個樣板寬高
	//int iStartX = ptMaxLoc.x - iTemplateW;
	//int iStartY = ptMaxLoc.y - iTemplateH;
	//int iEndX = ptMaxLoc.x + iTemplateW;

	//int iEndY = ptMaxLoc.y + iTemplateH;
	////塗黑
	//rectangle (matResult, Rect (iStartX, iStartY, 2 * iTemplateW * (1-dMaxOverlap * 2), 2 * iTemplateH * (1-dMaxOverlap * 2)), Scalar (dMinValue), CV_FILLED);
	////得到下一個最大值
	//Point ptNewMaxLoc;
	//minMaxLoc (matResult, 0, &dMaxValue, 0, &ptNewMaxLoc);
	//return ptNewMaxLoc;

	//比對到的區域需考慮重疊比例
	int iStartX = ptMaxLoc.x - iTemplateW * (1 - dMaxOverlap);
	int iStartY = ptMaxLoc.y - iTemplateH * (1 - dMaxOverlap);
	int iEndX = ptMaxLoc.x + iTemplateW * (1 - dMaxOverlap);

	int iEndY = ptMaxLoc.y + iTemplateH * (1 - dMaxOverlap);
	//塗黑
	rectangle (matResult, Rect (iStartX, iStartY, 2 * iTemplateW * (1 - dMaxOverlap), 2 * iTemplateH * (1 - dMaxOverlap)), Scalar (dMinValue), CV_FILLED);
	//得到下一個最大值
	Point ptNewMaxLoc;
	minMaxLoc (matResult, 0, &dMaxValue, 0, &ptNewMaxLoc);
	return ptNewMaxLoc;
}
void CAutoLikeDlg::FilterWithScore (vector<s_MatchParameter>* vec, double dScore)
{
	sort (vec->begin (), vec->end (), compareScoreBig2Small);
	int iSize = vec->size (), iIndexDelete = iSize + 1;
	for (int i = 0; i < iSize; i++)
	{
		if ((*vec)[i].dMatchScore < dScore)
		{
			iIndexDelete = i;
			break;
		}
	}
	if (iIndexDelete == iSize + 1)//沒有任何元素小於dScore
		return;
	vec->erase (vec->begin () + iIndexDelete, vec->end ());
	return;
	//刪除小於比對分數的元素
	vector<s_MatchParameter>::iterator it;
	for (it = vec->begin (); it != vec->end ();)
	{
		if (((*it).dMatchScore < dScore))
			it = vec->erase (it);
		else
			++it;
	}
}
void CAutoLikeDlg::LearnPattern (Mat matDst)
{
	m_TemplData.clear ();

	int iTopLayer = GetTopLayer (&matDst, (int)sqrt ((double)1024));
	buildPyramid (matDst, m_TemplData.vecPyramid, iTopLayer);
	s_TemplData* templData = &m_TemplData;

	int iSize = templData->vecPyramid.size ();
	templData->resize (iSize);

	for (int i = 0; i < iSize; i++)
	{
		double invArea = 1. / ((double)templData->vecPyramid[i].rows * templData->vecPyramid[i].cols);
		Scalar templMean, templSdv;
		double templNorm = 0, templSum2 = 0;

		meanStdDev (templData->vecPyramid[i], templMean, templSdv);
		templNorm = templSdv[0] * templSdv[0] + templSdv[1] * templSdv[1] + templSdv[2] * templSdv[2] + templSdv[3] * templSdv[3];

		if (templNorm < DBL_EPSILON)
		{
			templData->vecResultEqual1[i] = TRUE;
		}
		templSum2 = templNorm + templMean[0] * templMean[0] + templMean[1] * templMean[1] + templMean[2] * templMean[2] + templMean[3] * templMean[3];


		templSum2 /= invArea;
		templNorm = std::sqrt (templNorm);
		templNorm /= std::sqrt (invArea); // care of accuracy here


		templData->vecInvArea[i] = invArea;
		templData->vecTemplMean[i] = templMean;
		templData->vecTemplNorm[i] = templNorm;
	}
	templData->bIsPatternLearned = TRUE;
}
void CAutoLikeDlg::MatchTemplate (cv::Mat& matSrc, s_TemplData* pTemplData, cv::Mat& matResult, int iLayer)
{
	matchTemplate (matSrc, pTemplData->vecPyramid[iLayer], matResult, CV_TM_CCORR);
	CCOEFF_Denominator (matSrc, pTemplData, matResult, iLayer);
}
void CAutoLikeDlg::CCOEFF_Denominator (cv::Mat& matSrc, s_TemplData* pTemplData, cv::Mat& matResult, int iLayer)
{
	if (pTemplData->vecResultEqual1[iLayer])
	{
		matResult = Scalar::all (1);
		return;
	}
	double *q0 = 0, *q1 = 0, *q2 = 0, *q3 = 0;

	Mat sum, sqsum;
	integral (matSrc, sum, sqsum, CV_32F, CV_64F);

	double d2 = clock ();

	q0 = (double*)sqsum.data;
	q1 = q0 + pTemplData->vecPyramid[iLayer].cols;
	q2 = (double*)(sqsum.data + pTemplData->vecPyramid[iLayer].rows * sqsum.step);
	q3 = q2 + pTemplData->vecPyramid[iLayer].cols;

	float* p0 = (float*)sum.data;
	float* p1 = p0 + pTemplData->vecPyramid[iLayer].cols;
	float* p2 = (float*)(sum.data + pTemplData->vecPyramid[iLayer].rows*sum.step);
	float* p3 = p2 + pTemplData->vecPyramid[iLayer].cols;

	int sumstep = sum.data ? (int)(sum.step / sizeof (float)) : 0;
	int sqstep = sqsum.data ? (int)(sqsum.step / sizeof (double)) : 0;

	//
	double dTemplMean0 = pTemplData->vecTemplMean[iLayer][0];
	double dTemplNorm = pTemplData->vecTemplNorm[iLayer];
	double dInvArea = pTemplData->vecInvArea[iLayer];
	//

	int i, j;
	for (i = 0; i < matResult.rows; i++)
	{
		float* rrow = matResult.ptr<float> (i);
		int idx = i * sumstep;
		int idx2 = i * sqstep;

		for (j = 0; j < matResult.cols; j += 1, idx += 1, idx2 += 1)
		{
			double num = rrow[j], t;
			double wndMean2 = 0, wndSum2 = 0;

			t = p0[idx] - p1[idx] - p2[idx] + p3[idx];
			wndMean2 += t * t;
			num -= t * dTemplMean0;
			wndMean2 *= dInvArea;


			t = q0[idx2] - q1[idx2] - q2[idx2] + q3[idx2];
			wndSum2 += t;


			t = std::sqrt (MAX (wndSum2 - wndMean2, 0)) * dTemplNorm;

			if (fabs (num) < t)
				num /= t;
			else if (fabs (num) < t * 1.125)
				num = num > 0 ? 1 : -1;
			else
				num = 0;

			rrow[j] = (float)num;
		}
	}
}
void CAutoLikeDlg::GetRotatedROI (Mat& matSrc, Size size, Point2f ptLT, double dAngle, Mat& matROI)
{
	double dAngle_radian = dAngle * D2R;
	Point2f ptC ((matSrc.cols - 1) / 2.0f, (matSrc.rows - 1) / 2.0f);
	Point2f ptLT_rotate = ptRotatePt2f (ptLT, ptC, dAngle_radian);
	Size sizePadding (size.width + 6, size.height + 6);


	Mat rMat = getRotationMatrix2D (ptC, dAngle, 1);
	rMat.at<double> (0, 2) -= ptLT_rotate.x - 3;
	rMat.at<double> (1, 2) -= ptLT_rotate.y - 3;
	//平移旋轉矩陣(0, 2) (1, 2)的減，為旋轉後的圖形偏移，-= ptLT_rotate.x - 3 代表旋轉後的圖形往-X方向移動ptLT_rotate.x - 3
	//Debug

	//Debug
	warpAffine (matSrc, matROI, rMat, sizePadding);
}
Size CAutoLikeDlg::GetBestRotationSize (Size sizeSrc, Size sizeDst, double dRAngle)
{
	double dRAngle_radian = dRAngle * D2R;
	Point ptLT (0, 0), ptLB (0, sizeSrc.height - 1), ptRB (sizeSrc.width - 1, sizeSrc.height - 1), ptRT (sizeSrc.width - 1, 0);
	Point2f ptCenter ((sizeSrc.width - 1) / 2.0f, (sizeSrc.height - 1) / 2.0f);
	Point2f ptLT_R = ptRotatePt2f (Point2f (ptLT), ptCenter, dRAngle_radian);
	Point2f ptLB_R = ptRotatePt2f (Point2f (ptLB), ptCenter, dRAngle_radian);
	Point2f ptRB_R = ptRotatePt2f (Point2f (ptRB), ptCenter, dRAngle_radian);
	Point2f ptRT_R = ptRotatePt2f (Point2f (ptRT), ptCenter, dRAngle_radian);

	float fTopY = max (max (ptLT_R.y, ptLB_R.y), max (ptRB_R.y, ptRT_R.y));
	float fRightestX = max (max (ptLT_R.x, ptLB_R.x), max (ptRB_R.x, ptRT_R.x));

	if (fabs (fabs (dRAngle) - 90) < VISION_TOLERANCE || fabs (fabs (dRAngle) - 270) < VISION_TOLERANCE)
	{
		return Size (sizeSrc.height, sizeSrc.width);
	}
	else if (fabs (dRAngle) < VISION_TOLERANCE || fabs (fabs (dRAngle) - 180) < VISION_TOLERANCE)
	{
		return sizeSrc;
	}
	if (dRAngle > 360)
		dRAngle -= 360;
	else if (dRAngle < 0)
		dRAngle += 360;
	double dAngle = dRAngle;

	if (dAngle > 0 && dAngle < 90)
	{
		;
	}
	else if (dAngle > 90 && dAngle < 180)
	{
		dAngle -= 90;
	}
	else if (dAngle > 180 && dAngle < 270)
	{
		dAngle -= 180;
	}
	else if (dAngle > 270 && dAngle < 360)
	{
		dAngle -= 270;
	}
	else//Debug
	{
		AfxMessageBox (L"Unkown");
	}

	float fH1 = sizeDst.width * sin (dAngle * D2R) * cos (dAngle * D2R);
	float fH2 = sizeDst.height * sin (dAngle * D2R) * cos (dAngle * D2R);

	int iHalfHeight = (int)ceil (fTopY - ptCenter.y - fH1);
	int iHalfWidth = (int)ceil (fRightestX - ptCenter.x - fH2);

	return Size (iHalfWidth * 2, iHalfHeight * 2);
}
Point2f CAutoLikeDlg::ptRotatePt2f (Point2f ptInput, Point2f ptOrg, double dAngle)
{
	double dWidth = ptOrg.x * 2;
	double dHeight = ptOrg.y * 2;
	double dY1 = dHeight - ptInput.y, dY2 = dHeight - ptOrg.y;

	double dX = (ptInput.x - ptOrg.x) * cos (dAngle) - (dY1 - ptOrg.y) * sin (dAngle) + ptOrg.x;
	double dY = (ptInput.x - ptOrg.x) * sin (dAngle) + (dY1 - ptOrg.y) * cos (dAngle) + dY2;

	dY = -dY + dHeight;
	return Point2f ((float)dX, (float)dY);
}
void CAutoLikeDlg::FilterWithRotatedRect (vector<s_MatchParameter>* vec, int iMethod, double dMaxOverLap)
{
	int iMatchSize = (int)vec->size ();
	RotatedRect rect1, rect2;
	for (int i = 0; i < iMatchSize - 1; i++)
	{
		if (vec->at (i).bDelete)
			continue;
		for (int j = i + 1; j < iMatchSize; j++)
		{
			if (vec->at (j).bDelete)
				continue;
			rect1 = vec->at (i).rectR;
			rect2 = vec->at (j).rectR;
			vector<Point2f> vecInterSec;
			int iInterSecType = rotatedRectangleIntersection (rect1, rect2, vecInterSec);
			if (iInterSecType == INTERSECT_NONE)//無交集
				continue;
			else if (iInterSecType == INTERSECT_FULL) //一個矩形包覆另一個
			{
				int iDeleteIndex;
				if (iMethod == CV_TM_SQDIFF)
					iDeleteIndex = (vec->at (i).dMatchScore <= vec->at (j).dMatchScore) ? j : i;
				else
					iDeleteIndex = (vec->at (i).dMatchScore >= vec->at (j).dMatchScore) ? j : i;
				vec->at (iDeleteIndex).bDelete = TRUE;
			}
			else//交點 > 0
			{
				if (vecInterSec.size () < 3)//一個或兩個交點
					continue;
				else
				{
					int iDeleteIndex;
					//求面積與交疊比例
					SortPtWithCenter (vecInterSec);
					double dArea = contourArea (vecInterSec);
					double dRatio = dArea / rect1.size.area ();
					//若大於最大交疊比例，選分數高的
					if (dRatio > dMaxOverLap)
					{
						if (iMethod == CV_TM_SQDIFF)
							iDeleteIndex = (vec->at (i).dMatchScore <= vec->at (j).dMatchScore) ? j : i;
						else
							iDeleteIndex = (vec->at (i).dMatchScore >= vec->at (j).dMatchScore) ? j : i;
						vec->at (iDeleteIndex).bDelete = TRUE;
					}
				}
			}
		}
	}
	vector<s_MatchParameter>::iterator it;
	for (it = vec->begin (); it != vec->end ();)
	{
		if ((*it).bDelete)
			it = vec->erase (it);
		else
			++it;
	}
}
void CAutoLikeDlg::SortPtWithCenter (vector<Point2f>& vecSort)
{
	int iSize = (int)vecSort.size ();
	Point2f ptCenter;
	for (int i = 0; i < iSize; i++)
		ptCenter += vecSort[i];
	ptCenter /= iSize;

	Point2f vecX (1, 0);

	vector<pair<Point2f, double>> vecPtAngle (iSize);
	for (int i = 0; i < iSize; i++)
	{
		vecPtAngle[i].first = vecSort[i];//pt
		Point2f vec1 (vecSort[i].x - ptCenter.x, vecSort[i].y - ptCenter.y);
		float fNormVec1 = vec1.x * vec1.x + vec1.y * vec1.y;
		float fDot = vec1.x;

		if (vec1.y < 0)//若點在中心的上方
		{
			vecPtAngle[i].second = acos (fDot / fNormVec1) * R2D;
		}
		else if (vec1.y > 0)//下方
		{
			vecPtAngle[i].second = 360 - acos (fDot / fNormVec1) * R2D;
		}
		else//點與中心在相同Y
		{
			if (vec1.x - ptCenter.x > 0)
				vecPtAngle[i].second = 0;
			else
				vecPtAngle[i].second = 180;
		}

	}
	sort (vecPtAngle.begin (), vecPtAngle.end (), comparePtWithAngle);
	for (int i = 0; i < iSize; i++)
		vecSort[i] = vecPtAngle[i].first;
}
void CAutoLikeDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果將最小化按鈕加入您的對話方塊，您需要下列的程式碼，
// 以便繪製圖示。對於使用文件/檢視模式的 MFC 應用程式，
// 框架會自動完成此作業。

void CAutoLikeDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 繪製的裝置內容

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 將圖示置中於用戶端矩形
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 描繪圖示
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// 當使用者拖曳最小化視窗時，
// 系統呼叫這個功能取得游標顯示。
HCURSOR CAutoLikeDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CAutoLikeDlg::OnMouseMove (UINT nFlags, CPoint point)
{
	// TODO: 在此加入您的訊息處理常式程式碼和 (或) 呼叫預設值

	CDialogEx::OnMouseMove (nFlags, point);
}
Mat CAutoLikeDlg::GetScreenShot ()
{
	HWND hwnd = ::GetDesktopWindow ();
	HDC hwindowDC, hwindowCompatibleDC;

	int height, width, srcheight, srcwidth;
	HBITMAP hbwindow;
	Mat src;
	BITMAPINFOHEADER  bi;

	hwindowDC = ::GetDC (hwnd);
	hwindowCompatibleDC = CreateCompatibleDC (hwindowDC);
	SetStretchBltMode (hwindowCompatibleDC, COLORONCOLOR);

	RECT windowsize;    // get the height and width of the screen
	::GetClientRect (hwnd, &windowsize);

	srcheight = windowsize.bottom;
	srcwidth = windowsize.right;
	height = windowsize.bottom / 1;  //change this to whatever size you want to resize to
	width = windowsize.right / 1;

	src.create (height, width, CV_8UC3);     // This was problem

   // create a bitmap
	int iBits = GetDeviceCaps (hwindowDC, BITSPIXEL) * GetDeviceCaps (hwindowDC, PLANES);
	WORD wBitCount;
	if (iBits <= 1)
		wBitCount = 1;
	else if (iBits <= 4)
		wBitCount = 4;
	else if (iBits <= 8)
		wBitCount = 8;
	else
		wBitCount = 24;
	hbwindow = CreateCompatibleBitmap (hwindowDC, width, height);
	bi.biSize = sizeof (BITMAPINFOHEADER);    //http://msdn.microsoft.com/en-us/library/windows/window/dd183402%28v=vs.85%29.aspx
	bi.biWidth = width;
	bi.biHeight = -height;  //this is the line that makes it draw upside down or not
	bi.biPlanes = 1;
	bi.biBitCount = wBitCount;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 256;
	bi.biClrImportant = 0;



	// use the previously created device context with the bitmap
	SelectObject (hwindowCompatibleDC, hbwindow);
	// copy from the window device context to the bitmap device context
	StretchBlt (hwindowCompatibleDC, 0, 0, width, height, hwindowDC, 0, 0, srcwidth, srcheight, SRCCOPY); //change SRCCOPY to NOTSRCCOPY for wacky colors !
	GetDIBits (hwindowCompatibleDC, hbwindow, 0, height, src.data, (BITMAPINFO *)&bi, DIB_RGB_COLORS);  //copy from hwindowCompatibleDC to hbwindow


	// avoid memory leak
	DeleteObject (hbwindow);
	DeleteDC (hwindowCompatibleDC);
	::ReleaseDC (hwnd, hwindowDC);

	return src;
}

