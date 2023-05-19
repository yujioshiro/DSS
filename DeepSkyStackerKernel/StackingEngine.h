#pragma once

#include "RegisterEngine.h"
#include "PixelTransform.h"
#include "BackgroundCalibration.h"
#include "StackingTasks.h"

class CComputeOffsetTask;

/* ------------------------------------------------------------------- */

class	CImageCometShift
{
public :
	int						m_lImageIndex;
	double						m_fXShift,
								m_fYShift;

private :
	void	CopyFrom(const CImageCometShift & ics)
	{
		m_lImageIndex	= ics.m_lImageIndex;
		m_fXShift		= ics.m_fXShift;
		m_fYShift		= ics.m_fYShift;
	};

public :
	CImageCometShift(int lIndex = 0, double fXShift = 0, double fYShift = 0)
	{
		m_lImageIndex	= lIndex;
		m_fXShift		= fXShift;
		m_fYShift		= fYShift;
	};
	CImageCometShift(const CImageCometShift & ics)
	{
		CopyFrom(ics);
	};

	~CImageCometShift() {};

	CImageCometShift & operator = (const CImageCometShift & ics)
	{
		CopyFrom(ics);
		return (*this);
	};

	bool operator < (const CImageCometShift & ics) const
	{
		if (m_fXShift < ics.m_fXShift)
			return true;
		else if (m_fXShift > ics.m_fXShift)
			return false;
		else
			return (m_fYShift < ics.m_fYShift);
	};
};

typedef std::vector<CImageCometShift>		IMAGECOMETSHIFTVECTOR;

/* ------------------------------------------------------------------- */

class CLightFrameStackingInfo
{
public :
	fs::path m_strFileName;
	QString m_strInfoFileName;
	CBilinearParameters m_BilinearParameters;

private :
	void	CopyFrom(const CLightFrameStackingInfo & lfsi)
	{
		m_strFileName		 = lfsi.m_strFileName;
		m_strInfoFileName	 = lfsi.m_strInfoFileName;
		m_BilinearParameters = lfsi.m_BilinearParameters;
	};
public :
	CLightFrameStackingInfo() = default;
	CLightFrameStackingInfo(const fs::path& szFileName)
	{
		m_strFileName = szFileName;
	};

	~CLightFrameStackingInfo() {};

	CLightFrameStackingInfo(const CLightFrameStackingInfo & lfsi)
	{
		CopyFrom(lfsi);
	};

	CLightFrameStackingInfo & operator = (const CLightFrameStackingInfo & lfsi)
	{
		CopyFrom(lfsi);
		return (*this);
	};

	bool operator < (const CLightFrameStackingInfo & lfsi) const
	{
		return (m_strFileName.compare(lfsi.m_strFileName));
	};

};

typedef std::vector<CLightFrameStackingInfo>		LIGHTFRAMESTACKINGINFOVECTOR;
typedef LIGHTFRAMESTACKINGINFOVECTOR::iterator		LIGHTFRAMESTACKINGINFOITERATOR;

/* ------------------------------------------------------------------- */

class CLightFramesStackingInfo
{
private :
	fs::path m_strReferenceFrame;
	QString m_strStackingFileInfo;
	LIGHTFRAMESTACKINGINFOVECTOR m_vLightFrameStackingInfo;

private :
	void		GetInfoFileName(const fs::path& lightFrame, QString& strInfoFileName);

public :
	CLightFramesStackingInfo() {};
	virtual ~CLightFramesStackingInfo() {};

	void	SetReferenceFrame(const fs::path& szReferenceFrame);
	void	AddLightFrame(const fs::path& szLightFrame, const CBilinearParameters & bp);
	bool	GetParameters(const fs::path& szLightFrame, CBilinearParameters & bp);
	void	Save();
	void	Clear()
	{
		m_strReferenceFrame.clear();
		m_strStackingFileInfo.clear();
		m_vLightFrameStackingInfo.clear();
	};
};

/* ------------------------------------------------------------------- */
class CTaskInfo;
class CStackingEngine final
{
private:
	LIGHTFRAMEINFOVECTOR		m_vBitmaps;
	CLightFramesStackingInfo	m_StackingInfo;
	ProgressBase *				m_pProgress;
	fs::path m_strReferenceFrame;
	int						m_lNrCurrentStackable;
	std::atomic<int>		m_lNrStackable;
	std::atomic<int>		m_lNrCometStackable;
	int						m_lISOSpeed;
	int						m_lGain;
	QDateTime				m_DateTime;
	CBitmapExtraInfo			m_ExtraInfo;
	DSSRect						m_rcResult;
	double						m_fTotalExposure;
	std::shared_ptr<CMemoryBitmap> m_pOutput;
	std::shared_ptr<CMemoryBitmap> m_pEntropyCoverage;
	std::shared_ptr<CMemoryBitmap> m_pComet;
	IMAGECOMETSHIFTVECTOR		m_vCometShifts;
	double						m_fStarTrailsAngle;
	PIXELTRANSFORMVECTOR		m_vPixelTransforms;
	CBackgroundCalibration		m_BackgroundCalibration;
	std::shared_ptr<CMultiBitmap> m_pMasterLight;
	CTaskInfo *					m_pLightTask;
	int						m_lNrStacked;
	double						m_fKeptPercentage;
	bool						m_bSaveCalibrated;
	bool						m_bSaveIntermediate;
	bool						m_bSaveCalibratedDebayered;
	fs::path m_strCurrentLightFrame;
	CFATYPE						m_InputCFAType;
	int						m_lPixelSizeMultiplier;
	INTERMEDIATEFILEFORMAT		m_IntermediateFileFormat;
	bool						m_bCometStacking;
	bool						m_bCometInterpolating;
	bool						m_bCreateCometImage;
	bool						m_bSaveIntermediateCometImages;
	bool						m_bApplyFilterToCometImage;
	CPostCalibrationSettings	m_PostCalibrationSettings;
	bool						m_bChannelAlign;

	CComAutoCriticalSection		m_CriticalSection;

public:
	CStackingEngine() :
		m_pProgress { nullptr },
		m_lNrCurrentStackable { 0 },
		m_lNrStackable{ 0 },
		m_lNrCometStackable{ 0 },
		m_lISOSpeed{ 0 },
		m_lGain{ -1 },
		m_fStarTrailsAngle {0.0},
		m_pLightTask{ nullptr },
		m_lNrStacked{ 0 },
		m_fTotalExposure{ 0 },
		m_fKeptPercentage{ 100.0 },
		m_bSaveCalibrated{ CAllStackingTasks::GetSaveCalibrated() },
		m_bSaveCalibratedDebayered{ CAllStackingTasks::GetSaveCalibratedDebayered() },
		m_bSaveIntermediate{ CAllStackingTasks::GetCreateIntermediates() },
		m_InputCFAType{ CFATYPE_NONE },
		m_lPixelSizeMultiplier{ CAllStackingTasks::GetPixelSizeMultiplier() },
		m_IntermediateFileFormat{ CAllStackingTasks::GetIntermediateFileFormat() },
		m_bCometStacking{ false },
		m_bCreateCometImage{ false },
		m_bSaveIntermediateCometImages{ CAllStackingTasks::GetSaveIntermediateCometImages() },
		m_bApplyFilterToCometImage{ CAllStackingTasks::GetApplyMedianFilterToCometImage() },
		m_bChannelAlign{ CAllStackingTasks::GetChannelAlign() },
		m_bCometInterpolating{ false }

	{
		CAllStackingTasks::GetPostCalibrationSettings(m_PostCalibrationSettings);
	}

	~CStackingEngine() = default;

	bool ComputeLightFrameOffset(int lBitmapIndice);
	void OverrideIntermediateFileFormat(INTERMEDIATEFILEFORMAT fmt) { m_IntermediateFileFormat = fmt; }
	inline void incStackable() { ++m_lNrStackable; }
	inline void incCometStackableIfBitmapHasComet(const int n) {
		if (this->m_vBitmaps[n].m_bComet)
			++m_lNrCometStackable;
	}

private:
	bool	AddLightFramesToList(CAllStackingTasks & tasks);
	void	ComputeMissingCometPositions();
	void	ComputeOffsets();
	bool	IsLightFrameStackable(LPCTSTR szFile);
	bool	RemoveNonStackableLightFrames(CAllStackingTasks & tasks);
	void	GetResultISOSpeed();
	void	GetResultGain();
	void	GetResultDateTime();
	void	GetResultExtraInfo();
	DSSRect	computeLargestRectangle();
	bool	computeSmallestRectangle(DSSRect & rc);
	int	FindBitmapIndex(LPCTSTR szFile);
	void	ComputeBitmap();
	std::shared_ptr<CMultiBitmap> CreateMasterLightMultiBitmap(const CMemoryBitmap* pInBitmap, const bool bColor);
	bool StackAll(CAllStackingTasks & tasks, std::shared_ptr<CMemoryBitmap>& rpBitmap);
	template <class T>
	std::pair<bool, T> StackLightFrame(std::shared_ptr<CMemoryBitmap> pBitmap, CPixelTransform& PixTransform, double fExposure, bool bComet, T futureForWrite);
	bool	AdjustEntropyCoverage();
	bool	AdjustBayerDrizzleCoverage();
	bool	SaveCalibratedAndRegisteredLightFrame(CMemoryBitmap * pBitmap) const;
	bool	SaveCalibratedLightFrame(std::shared_ptr<CMemoryBitmap> pBitmap) const;
	bool	SaveDeltaImage(CMemoryBitmap* pBitmap) const;
	bool	SaveCometImage(CMemoryBitmap* pBitmap) const;
	bool	SaveCometlessImage(CMemoryBitmap* pBitmap) const;
	TRANSFORMATIONTYPE GetTransformationType();

public:
	CLightFrameInfo& getBitmap(const int n)
	{
		return this->m_vBitmaps[n];
	}

	void SetReferenceFrame(const fs::path& szRefFrame)
	{
		m_strReferenceFrame = szRefFrame;
	}

	void SetSaveIntermediate(bool bSaveIntermediate)
	{
		m_bSaveIntermediate = bSaveIntermediate;
	}

	void SetSaveCalibrated(bool bSaveCalibrated)
	{
		m_bSaveCalibrated= bSaveCalibrated;
	}

	void SetKeptPercentage(double fPercent)
	{
		m_fKeptPercentage = fPercent;
	}

	void ComputeOffsets(CAllStackingTasks& tasks, ProgressBase* pProgress);
	bool StackLightFrames(CAllStackingTasks& tasks, ProgressBase* const pProgress, std::shared_ptr<CMemoryBitmap>& rpBitmap);

	LIGHTFRAMEINFOVECTOR& LightFrames()
	{
		return m_vBitmaps;
	}

	void SetCometInterpolating(bool bSet)
	{
		m_bCometInterpolating = bSet;
	}

	bool GetDefaultOutputFileName(fs::path& strFileName, const fs::path& szFileList, bool bTIFF = true);
	void WriteDescription(CAllStackingTasks& tasks, const fs::path& outputFile);
};

