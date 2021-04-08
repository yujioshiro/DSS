#ifndef __MEDIANFILTERENGINE_H__
#define __MEDIANFILTERENGINE_H__

#include "avx_filter.h"

/* ------------------------------------------------------------------- */

template <typename TType>
class CInternalMedianFilterEngineT
{
public :
	TType* m_pvInValues;
	TType* m_pvOutValues;
	int m_lWidth;
	int m_lHeight;
	CFATYPE m_CFAType;
	int m_lFilterSize;

	template <typename TType>
	class CFilterTask : public CMultitask
	{
	private :
		CInternalMedianFilterEngineT<TType>* m_pEngine;
		CDSSProgress* m_pProgress;

	public :
		CFilterTask()
		{}

		virtual ~CFilterTask()
		{}

		void Init(CInternalMedianFilterEngineT<TType>* pEngine, CDSSProgress* pProgress)
		{
			m_pEngine = pEngine;
			m_pProgress = pProgress;
		}

		virtual bool DoTask(HANDLE hEvent) override
		{
			bool bResult = true;
			bool bEnd = false;
			MSG msg;
			const int lWidth = m_pEngine->m_lWidth;
			const int lHeight = m_pEngine->m_lHeight;
			const int lFilterSize = m_pEngine->m_lFilterSize;
			const CFATYPE CFAType = m_pEngine->m_CFAType;

			std::vector<TType> vValues;
			vValues.reserve((m_pEngine->m_lFilterSize * 2 + 1) * (m_pEngine->m_lFilterSize * 2 + 1));
			AvxImageFilter avxFilter(m_pEngine);

			// Create a message queue and signal the event
			PeekMessage(&msg, nullptr, 0, 0, PM_NOREMOVE);
			SetEvent(hEvent);
			while (!bEnd && GetMessage(&msg, nullptr, 0, 0))
			{
				if (msg.message == WM_MT_PROCESS)
				{
					if (avxFilter.filter(msg.wParam, msg.wParam + msg.lParam) != 0)
					{
						if (CFAType != CFATYPE_NONE)
						{
							TType* pOutValues = m_pEngine->m_pvOutValues;

							pOutValues += msg.wParam * lWidth;

							for (int j = msg.wParam; j < msg.wParam + msg.lParam; j++)
							{
								for (int i = 0; i < lWidth; i++)
								{
									// Compute the min and max values in X and Y
									int lXMin, lXMax, lYMin, lYMax;
									BAYERCOLOR BayerColor = GetBayerColor(i, j, CFAType);

									lXMin = std::max(0, i - lFilterSize);
									lXMax = std::min(i + lFilterSize, lWidth - 1);
									lYMin = std::max(0, j - lFilterSize);
									lYMax = std::min(j + lFilterSize, lHeight - 1);

									// Fill the array with the values
									TType* pInLine = m_pEngine->m_pvInValues;
									pInLine += lXMin + (lYMin * lWidth);
									vValues.resize(0);
									for (int k = lYMin; k <= lYMax; k++)
									{
										TType* pInValues = pInLine;

										for (int l = lXMin; l <= lXMax; l++)
										{
											if (GetBayerColor(l, k, CFAType) == BayerColor)
												vValues.push_back(*pInValues);
											pInValues++;
										};
										pInLine += lWidth;
									};

									TType			fMedian = Median(vValues);

									*pOutValues = fMedian;
									pOutValues++;
								};
							};
						}
						else
						{
							TType* pOutValues = m_pEngine->m_pvOutValues;

							pOutValues += msg.wParam * lWidth;

							for (int j = msg.wParam; j < msg.wParam + msg.lParam; j++)
							{
								for (int i = 0; i < lWidth; i++)
								{
									// Compute the min and max values in X and Y
									int lXMin, lXMax, lYMin, lYMax;

									lXMin = std::max(0, i - lFilterSize);
									lXMax = std::min(i + lFilterSize, lWidth - 1);
									lYMin = std::max(0, j - lFilterSize);
									lYMax = std::min(j + lFilterSize, lHeight - 1);

									vValues.resize((lXMax - lXMin + 1) * (lYMax - lYMin + 1));

									// Fill the array with the values
									TType* pInValues = m_pEngine->m_pvInValues;
									TType* pAreaValues = &(vValues[0]);
									pInValues += lXMin + (lYMin * lWidth);
									for (int k = lYMin; k <= lYMax; k++)
									{
										memcpy(pAreaValues, pInValues, sizeof(TType) * (lXMax - lXMin + 1));
										pInValues += lWidth;
										pAreaValues += lXMax - lXMin + 1;
									};

									*pOutValues = Median(vValues);
									pOutValues++;
								};
							};
						};
					};

					SetEvent(hEvent);
				}
				else if (msg.message == WM_MT_STOP)
					bEnd = true;
			};

			return true;
		};

		virtual bool Process() override
		{
			bool bResult = true;
			const int lHeight = m_pEngine->m_lHeight;
			int i = 0;
			int lStep;
			int lRemaining;

			if (m_pProgress)
				m_pProgress->SetNrUsedProcessors(GetNrThreads());
			lStep = std::max(1, lHeight / 50);
			lRemaining = lHeight;

			while (i < lHeight)
			{
				const int lAdd = std::min(lStep, lRemaining);
				const auto dwThreadId = GetAvailableThreadId();
				PostThreadMessage(dwThreadId, WM_MT_PROCESS, i, lAdd);

				i += lAdd;
				lRemaining -= lAdd;
				if (m_pProgress)
					m_pProgress->Progress2(nullptr, i);
			};

			CloseAllThreads();

			if (m_pProgress)
				m_pProgress->SetNrUsedProcessors();

			return bResult;
		};
	};

	friend CFilterTask<TType>;

public :
	CInternalMedianFilterEngineT()
    {
        m_lWidth = 0;
        m_lHeight = 0;
        m_lFilterSize = 0;
    };
	virtual ~CInternalMedianFilterEngineT() {};

	bool	ApplyFilter(CDSSProgress * pProgress);
};

/* ------------------------------------------------------------------- */

template <typename TType>
inline bool CInternalMedianFilterEngineT<TType>::ApplyFilter(CDSSProgress * pProgress)
{
	bool					bResult = true;

	if (pProgress)
		pProgress->Start2(nullptr, m_lHeight);

	CFilterTask<TType>		FilterTask;

	FilterTask.Init(this, pProgress);
	FilterTask.StartThreads();
	FilterTask.Process();

	if (pProgress)
		pProgress->End2();

/*
	std::vector<TType>		vValues;

	vValues.reserve((m_lFilterSize*2+1)*(m_lFilterSize*2+1));

	if (pProgress)
		pProgress->Start2(nullptr, m_lHeight);

	if (m_CFAType != CFATYPE_NONE)
	{
		TType *				pOutValues = m_pvOutValues;

		for (int j = 0;j<m_lHeight;j++)
		{
			for (int i = 0;i<m_lWidth;i++)
			{
				// Compute the min and max values in X and Y
				int			lXMin, lXMax,
								lYMin, lYMax;
				BAYERCOLOR		BayerColor = GetBayerColor(i, j, m_CFAType);

				lXMin = max(0, i-m_lFilterSize);
				lXMax = min(i+m_lFilterSize, m_lWidth-1);
				lYMin = max(0, j-m_lFilterSize);
				lYMax = min(j+m_lFilterSize, m_lHeight-1);

				// Fill the array with the values
				TType *			pInLine   = m_pvInValues;
				pInLine += lXMin + (lYMin * m_lWidth);
				vValues.resize(0);
				for (int k = lYMin;k<=lYMax;k++)
				{
					TType *		pInValues = pInLine;

					for (int l = lXMin;l<=lXMax;l++)
					{
						if (GetBayerColor(l, k, m_CFAType) == BayerColor)
							vValues.push_back(*pInValues);
						pInValues++;
					};
					pInLine   += m_lWidth;
				};

				TType			fMedian = Median(vValues);

				*pOutValues = fMedian;
				pOutValues++;
			};

			if (pProgress)
				pProgress->Progress2(nullptr, j+1);
		};
	}
	else
	{
		TType *				pOutValues = m_pvOutValues;

		for (int j = 0;j<m_lHeight;j++)
		{
			for (int i = 0;i<m_lWidth;i++)
			{
				// Compute the min and max values in X and Y
				int			lXMin, lXMax,
								lYMin, lYMax;

				lXMin = max(0, i-m_lFilterSize);
				lXMax = min(i+m_lFilterSize, m_lWidth-1);
				lYMin = max(0, j-m_lFilterSize);
				lYMax = min(j+m_lFilterSize, m_lHeight-1);

				vValues.resize((lXMax-lXMin+1)*(lYMax-lYMin+1));

				// Fill the array with the values
				TType *			pInValues   = m_pvInValues;
				TType *			pAreaValues = &(vValues[0]);
				pInValues += lXMin + (lYMin * m_lWidth);
				for (int k = lYMin;k<=lYMax;k++)
				{
					memcpy(pAreaValues, pInValues, sizeof(TType) * (lXMax-lXMin+1));
					pInValues   += m_lWidth;
					pAreaValues += lXMax-lXMin+1;
				};

				TType			fMedian = Median(vValues);

				*pOutValues = fMedian;
				pOutValues ++;
			};

			if (pProgress)
				pProgress->Progress2(nullptr, j+1);
		};
	};

	if (pProgress)
		pProgress->End2();
*/
	return bResult;
};

/* ------------------------------------------------------------------- */

template <typename TType>
inline bool	CGrayMedianFilterEngineT<TType>::GetFilteredImage(CMemoryBitmap ** ppOutBitmap, int lFilterSize, CDSSProgress * pProgress)
{
	bool bResult = false;

	// Create Output Bitmap from Input Bitmap
	if (m_pInBitmap != nullptr)
	{
		CSmartPtr<CGrayBitmapT<TType>> pOutBitmap;
		pOutBitmap.Attach(dynamic_cast<CGrayBitmapT<TType>*>(m_pInBitmap->Clone()));

		if (pOutBitmap)
		{
			CInternalMedianFilterEngineT<TType>	InternalFilter;

			InternalFilter.m_pvInValues  = &(m_pInBitmap->m_vPixels[0]);
			InternalFilter.m_pvOutValues = &(pOutBitmap->m_vPixels[0]);
			InternalFilter.m_lWidth      = m_pInBitmap->m_lWidth;
			InternalFilter.m_lHeight	 = m_pInBitmap->m_lHeight;
			InternalFilter.m_CFAType	 = m_pInBitmap->m_CFAType;
			if (InternalFilter.m_CFAType != CFATYPE_NONE)
				lFilterSize *= 2;
			InternalFilter.m_lFilterSize = lFilterSize;

			bResult = InternalFilter.ApplyFilter(pProgress);

			if (bResult)
			{
				CSmartPtr<CMemoryBitmap> pOutBitmap2;
				pOutBitmap2 = pOutBitmap;
				pOutBitmap2.CopyTo(ppOutBitmap);
			};
		};
	};

	return bResult;
};

/* ------------------------------------------------------------------- */

template <typename TType>
inline bool	CColorMedianFilterEngineT<TType>::GetFilteredImage(CMemoryBitmap** ppOutBitmap, int lFilterSize, CDSSProgress* pProgress)
{
	bool bResult = false;

	// Create Output Bitmap from Input Bitmap
	if (m_pInBitmap != nullptr)
	{
		CSmartPtr<CColorBitmapT<TType>> pOutBitmap;
		pOutBitmap.Attach(dynamic_cast<CColorBitmapT<TType> *> (m_pInBitmap->Clone()));

		if (pOutBitmap)
		{
			CInternalMedianFilterEngineT<TType>	InternalFilter;

			InternalFilter.m_lWidth      = m_pInBitmap->m_lWidth;
			InternalFilter.m_lHeight	 = m_pInBitmap->m_lHeight;
			InternalFilter.m_lFilterSize = lFilterSize;
			InternalFilter.m_CFAType	 = CFATYPE_NONE;

			InternalFilter.m_pvInValues  = &(m_pInBitmap->m_Red.m_vPixels[0]);
			InternalFilter.m_pvOutValues = &(pOutBitmap->m_Red.m_vPixels[0]);
			bResult = InternalFilter.ApplyFilter(pProgress);

			InternalFilter.m_pvInValues  = &(m_pInBitmap->m_Green.m_vPixels[0]);
			InternalFilter.m_pvOutValues = &(pOutBitmap->m_Green.m_vPixels[0]);
			bResult = InternalFilter.ApplyFilter(pProgress);

			InternalFilter.m_pvInValues  = &(m_pInBitmap->m_Blue.m_vPixels[0]);
			InternalFilter.m_pvOutValues = &(pOutBitmap->m_Blue.m_vPixels[0]);
			bResult = InternalFilter.ApplyFilter(pProgress);

			if (bResult)
			{
				CSmartPtr<CMemoryBitmap> pOutBitmap2;
				pOutBitmap2 = pOutBitmap;
				pOutBitmap2.CopyTo(ppOutBitmap);
			};
		};
	};

	return bResult;
};

/* ------------------------------------------------------------------- */

#endif // __MEDIANFILTERENGINE_H__
