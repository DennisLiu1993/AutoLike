
// AutoLikeDlg.h: 標頭檔
//

#pragma once
#include "opencv2/opencv.hpp"
using namespace std;
using namespace cv;


struct s_TemplData
{
	vector<Mat> vecPyramid;
	vector<Scalar> vecTemplMean;
	vector<double> vecTemplNorm;
	vector<double> vecInvArea;
	vector<BOOL> vecResultEqual1;
	BOOL bIsPatternLearned;
	void clear ()
	{
		vector<Mat> ().swap (vecPyramid);
		vector<double> ().swap (vecTemplNorm);
		vector<double> ().swap (vecInvArea);
		vector<Scalar> ().swap (vecTemplMean);
		vector<BOOL> ().swap (vecResultEqual1);
	}
	void resize (int iSize)
	{
		vecTemplMean.resize (iSize);
		vecTemplNorm.resize (iSize, 0);
		vecInvArea.resize (iSize, 1);
		vecResultEqual1.resize (iSize, FALSE);
	}
	s_TemplData ()
	{
		bIsPatternLearned = FALSE;
	}
};
struct s_MatchParameter
{
	Point2d pt;
	double dMatchScore;
	double dMatchAngle;
	//Mat matRotatedSrc;
	Rect rectRoi;
	double dAngleStart;
	double dAngleEnd;
	RotatedRect rectR;
	Rect rectBounding;
	BOOL bDelete;

	double vecResult[3][3];//for subpixel
	int iMaxScoreIndex;//for subpixel
	BOOL bPosOnBorder;
	Point2d ptSubPixel;
	double dNewAngle;

	s_MatchParameter (Point2f ptMinMax, double dScore, double dAngle)//, Mat matRotatedSrc = Mat ())
	{
		pt = ptMinMax;
		dMatchScore = dScore;
		dMatchAngle = dAngle;

		bDelete = FALSE;
		dNewAngle = 0.0;

		bPosOnBorder = FALSE;
	}
	s_MatchParameter ()
	{
		double dMatchScore = 0;
		double dMatchAngle = 0;
	}
	~s_MatchParameter ()
	{

	}
};
// CAutoLikeDlg 對話方塊
class CAutoLikeDlg : public CDialogEx
{
// 建構
public:
	CAutoLikeDlg(CWnd* pParent = nullptr);	// 標準建構函式

// 對話方塊資料
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_AUTOLIKE_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支援


// 程式碼實作
protected:
	HICON m_hIcon;

	// 產生的訊息對應函式
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnMouseMove (UINT nFlags, CPoint point);
	Mat GetScreenShot ();
	afx_msg void OnBnClickedButtonExecute ();
	int m_iOffsetX;
	int m_iOffsetY;
	Mat m_matEmogi;


	void LeftClick ();
	void RightClick ();
	void MouseMove (int x, int y);
	//Match
	s_TemplData m_TemplData;
	BOOL Match (Mat matSrc, Mat matDst, Rect rectRoi);
	void LearnPattern (Mat matDst);
	int GetTopLayer (Mat* matTempl, int iMinDstLength);
	void MatchTemplate (cv::Mat& matSrc, s_TemplData* pTemplData, cv::Mat& matResult, int iLayer);
	void GetRotatedROI (Mat& matSrc, Size size, Point2f ptLT, double dAngle, Mat& matROI);
	void CCOEFF_Denominator (cv::Mat& matSrc, s_TemplData* pTemplData, cv::Mat& matResult, int iLayer);
	Size  GetBestRotationSize (Size sizeSrc, Size sizeDst, double dRAngle);
	Point2f ptRotatePt2f (Point2f ptInput, Point2f ptOrg, double dAngle);
	void FilterWithScore (vector<s_MatchParameter>* vec, double dScore);
	void FilterWithRotatedRect (vector<s_MatchParameter>* vec, int iMethod = CV_TM_CCOEFF_NORMED, double dMaxOverLap = 0);
	Point GetNextMaxLoc (Mat & matResult, Point ptMaxLoc, double dMinValue, int iTemplateW, int iTemplateH, double& dMaxValue, double dMaxOverlap);
	void SortPtWithCenter (vector<Point2f>& vecSort);
	//vector<s_SingleTargetMatch> m_vecSingleTargetData;
	CButton m_ckDebug;
	CButton m_ckClick;
};
