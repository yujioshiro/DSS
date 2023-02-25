#include <stdafx.h>
#include "resource.h"
#include "BitmapBase.h"
#include "DSSTools.h"
#include "MultiBitmapProcess.h"
#include "StackingTasks.h"
#include "DSSProgress.h"
#include <algorithm>
#include <iostream>
#include "Multitask.h"
#include "avx_output.h"
#include <omp.h>
#include <mutex>

/* ------------------------------------------------------------------- */

static void GetTempFileName(CString & strFile)
{
	TCHAR			szTempFileName[1+_MAX_PATH];
	QString			strFolder(CAllStackingTasks::GetTemporaryFilesFolder());

	GetTempFileName(CString((LPCTSTR)strFolder.utf16()), _T("DSS"), 0, szTempFileName);

	strFile = szTempFileName;
};

/* ------------------------------------------------------------------- */
/* ------------------------------------------------------------------- */

void CMultiBitmap::SetBitmapModel(const CMemoryBitmap* pBitmap)
{
	m_pBitmapModel = pBitmap->Clone(true);
}

/* ------------------------------------------------------------------- */

void CMultiBitmap::DestroyTempFiles()
{
	for (int i = 0;i<m_vFiles.size();i++)
	{
		if (m_vFiles[i].m_strFile.GetLength())
			DeleteFile(m_vFiles[i].m_strFile);
		m_vFiles[i].m_strFile.Empty();
	};
	m_vFiles.clear();
};

/* ------------------------------------------------------------------- */

void CMultiBitmap::InitParts()
{
	ZFUNCTRACE_RUNTIME();

	// make files a maximum of 50 Mb

	const int lLineSize = (GetNrBytesPerChannel() * GetNrChannels() * m_lWidth);

	const int lNrLinesPerFile = 50000000L / lLineSize;
	int lNrLines = lNrLinesPerFile / m_lNrBitmaps;
	int lNrRemainingLines = lNrLinesPerFile % m_lNrBitmaps;

	if (lNrLines == 0)
		lNrLines = 1;

	int lNrParts = m_lNrBitmaps * m_lHeight / lNrLinesPerFile;
	if ((m_lNrBitmaps * m_lHeight) % lNrLinesPerFile != 0)
		lNrParts++;

	m_vFiles.clear();

	int lStartRow = -1;
	int lEndRow = -1;

	while (lEndRow < m_lHeight - 1)
	{
		CString strFile;
		GetTempFileName(strFile);

		lStartRow = lEndRow + 1;
		lEndRow = lStartRow + lNrLines;
		if (lNrRemainingLines != 0)
		{
			lEndRow++;
			lNrRemainingLines--;
		}
		lEndRow = std::min(lEndRow, m_lHeight - 1);

		m_vFiles.emplace_back(strFile, lStartRow, lEndRow);
	}

	m_bInitDone.store(true);
}


// Save the bitmap to the temporary file
bool CMultiBitmap::AddBitmap(CMemoryBitmap* pBitmap, ProgressBase* pProgress)
{
	static std::mutex initMutex{};

	ZFUNCTRACE_RUNTIME();

	if (m_bInitDone.load() == false)
	{
		auto lock = std::scoped_lock{ initMutex };
		if (m_bInitDone.load() == false)
		{
			m_lWidth = pBitmap->RealWidth();
			m_lHeight = pBitmap->RealHeight();
			InitParts(); // Will set m_bInitDone to true
			m_lNrAddedBitmaps = 0;
		}
	}

	// Save the bitmap to the file
	const size_t lScanLineSize = static_cast<size_t>(pBitmap->BitPerSample()) * (pBitmap->IsMonochrome() ? 1 : 3) * m_lWidth / 8;
	std::vector<std::uint8_t> scanLineBuffer(lScanLineSize);

	if (pProgress)
		pProgress->Start2(m_lHeight);

	for (const auto& file : m_vFiles)
	{
		auto dtor = [](FILE* fp) { if (fp != nullptr) fclose(fp); };
		std::unique_ptr<FILE, decltype(dtor)> pFile{ _tfopen(file.m_strFile, _T("a+b")), dtor };
		if (pFile.get() == nullptr)
			return false;

		fseek(pFile.get(), 0, SEEK_END);

		for (int j = file.m_lStartRow; j <= file.m_lEndRow; j++)
		{
			pBitmap->GetScanLine(j, scanLineBuffer.data());
			if (fwrite(scanLineBuffer.data(), lScanLineSize, 1, pFile.get()) != 1)
				return false;

			if (pProgress)
				pProgress->Progress2(j + 1);
		}
	}

	if (pProgress)
		pProgress->End2();

	m_lNrAddedBitmaps++;

	return true;
}

/* ------------------------------------------------------------------- */

class CCombineTask
{
private:
	int							m_lStartRow;
	int							m_lEndRow;
	size_t						m_lScanLineSize;
	ProgressBase*				m_pProgress;
	CMultiBitmap*				m_pMultiBitmap;
	void*						m_pBuffer;
	CMemoryBitmap* m_pBitmap;

public:
    CCombineTask(int startRow, int endRow, size_t scanLineSize, void* pBuffer, ProgressBase* pProgress, CMultiBitmap* pMultiBitmap, CMemoryBitmap* pBitmap) :
		m_lStartRow{ startRow },
		m_lEndRow{ endRow },
		m_lScanLineSize{ scanLineSize },
		m_pProgress{ pProgress },
		m_pMultiBitmap{ pMultiBitmap },
        m_pBuffer{ pBuffer },
		m_pBitmap{ pBitmap }
	{}

	~CCombineTask() = default;

	void process();
};

void CCombineTask::process()
{
	ZFUNCTRACE_RUNTIME();
	const int nrProcessors = CMultitask::GetNrProcessors();
	const int nrRows = m_lEndRow - m_lStartRow + 1;
	const size_t nrBitmaps = m_pMultiBitmap->GetNrAddedBitmaps();
	int progress = m_lStartRow;
	bool stop = false;

	std::vector<void*> scanLines(nrBitmaps, nullptr);
	AvxOutputComposition avxOutputComposition(*m_pMultiBitmap, *m_pBitmap);

	const auto handleError = [](const auto& errorMessage, const auto flags) -> void
	{
#if defined(_CONSOLE)
		std::wcerr << errorMessage;
#else
		AfxMessageBox(errorMessage, flags);
#endif
		exit(1);
	};

#pragma omp parallel for default(none) shared(stop) firstprivate(scanLines, avxOutputComposition) if(nrProcessors > 1 && nrRows > 1) // No "schedule" clause gives fastest result.
	for (int row = m_lStartRow; row <= m_lEndRow; ++row)
	{
		if (stop)
			continue;

		try
		{
			if (omp_get_thread_num() == 0 && m_pProgress != nullptr)
			{
				stop = m_pProgress->IsCanceled();
				m_pProgress->Progress2(progress += nrProcessors);
			}

			for (size_t k = 0, offset = (row - m_lStartRow) * m_lScanLineSize; k < nrBitmaps; ++k, offset += nrRows * m_lScanLineSize)
				scanLines[k] = static_cast<char*>(m_pBuffer) + offset;

			// First try AVX accelerated code, if not supported -> run conventional code.
			if (avxOutputComposition.compose(row, scanLines) != 0)
				m_pMultiBitmap->SetScanLines(m_pBitmap, row, scanLines);
		}
		catch (const std::exception& e)
		{
			CString errorMessage(static_cast<LPCTSTR>(CA2CT(e.what())));
			handleError(errorMessage, MB_OK | MB_ICONSTOP);
		}
#if !defined(_CONSOLE)
		catch (CException& e)
		{
			e.ReportError();
			e.Delete();
			exit(1);
		}
#endif
		catch (ZException& ze)
		{
			auto location = ze.locationAtIndex(0);
			CString errorMessage;
			errorMessage.Format(_T("Exception %s thrown from %s Function: %s() Line: %lu\n\n%s"),
				CString{ CA2CT(ze.name()) }, CString{ CA2CT(location->fileName()) }, CString{ CA2CT(location->functionName()) }, location->lineNumber(), CString{ CA2CT(ze.text(0)) });
			handleError(errorMessage, MB_OK | MB_ICONSTOP);
		}
		catch (...)
		{
			CString errorMessage(_T("Unknown exception caught"));
			handleError(errorMessage, MB_OK | MB_ICONSTOP);
		}
	}
}

/* ------------------------------------------------------------------- */

static void ComputeWeightedAverage(int x, int y, CMemoryBitmap* pBitmap, CMemoryBitmap* pHomBitmap, CMemoryBitmap* pOutBitmap)
{
	//ZFUNCTRACE_RUNTIME();
	bool bColor = !pBitmap->IsMonochrome();
	int lWidth = pBitmap->Width();
	int lHeight = pBitmap->Height();

	if (bColor)
	{
		double		fRed = 0, fGreen = 0, fBlue = 0;
		double		fWRed = 0, fWGreen = 0, fWBlue = 0;

		for (int i = std::max(0, x-5);i<=min(lWidth-1, x+5);i++)
		{
			for (int j = std::max(0, y-5);j<=min(lHeight-1, y+5);j++)
			{
				double		fRed1, fGreen1, fBlue1;
				double		fWRed1, fWGreen1, fWBlue1;

				pBitmap->GetPixel(i, j, fRed1, fGreen1, fBlue1);
				pHomBitmap->GetPixel(i, j, fWRed1, fWGreen1, fWBlue1);

				fRed	+= fRed1/(1.0+fWRed1);
				fGreen	+= fGreen1/(1.0+fWGreen1);
				fBlue	+= fBlue1/(1.0+fWBlue1);

				fWRed   += 1.0/(1.0+fWRed1);
				fWGreen += 1.0/(1.0+fWGreen1);
				fWBlue  += 1.0/(1.0+fWBlue1);
			};
		};

		fRed   /= fWRed;
		fGreen /= fWGreen;
		fBlue  /= fWBlue;

		pOutBitmap->SetPixel(x, y, fRed, fGreen, fBlue);
	}
	else
	{
		double		fGray = 0;
		double		fWGray = 0;

		for (int i = std::max(0, x-5);i<=min(lWidth-1, x+5);i++)
		{
			for (int j = std::max(0, y-5);j<=min(lHeight-1, y+5);j++)
			{
				double		fGray1;
				double		fWGray1;

				pBitmap->GetPixel(i, j, fGray1);
				pHomBitmap->GetPixel(i, j, fWGray1);

				fGray	+= fGray1/(1.0+fWGray1);

				fWGray   += 1.0/(1.0+fWGray1);
			};
		};

		fGray   /= fWGray;

		pOutBitmap->SetPixel(x, y, fGray);
	};
};

std::shared_ptr<CMemoryBitmap> CMultiBitmap::SmoothOut(CMemoryBitmap* pBitmap, ProgressBase* const pProgress) const
{
	if (static_cast<bool>(m_pHomBitmap))
	{
		std::shared_ptr<CMemoryBitmap> pOutBitmap{ pBitmap->Clone() };

		if (pProgress != nullptr)
			pProgress->Start2(m_lWidth);

		for (int i = 0; i < m_lWidth; ++i)
		{
			if (pProgress != nullptr)
				pProgress->Progress2(i);

			for (int j = 0; j < m_lHeight; ++j)
			{
				// Compute the weighted average of a 11x11 area around each pixel
				// It can be lengthy!
				ComputeWeightedAverage(i, j, pBitmap, m_pHomBitmap.get(), pOutBitmap.get());
			}
		}

		if (pProgress != nullptr)
			pProgress->End2();

		return pOutBitmap;
	}
	return std::shared_ptr<CMemoryBitmap>{};
}

std::shared_ptr<CMemoryBitmap> CMultiBitmap::GetResult(ProgressBase* pProgress)
{
	ZFUNCTRACE_RUNTIME();
	std::shared_ptr<CMemoryBitmap> pBitmap;

	if (m_bInitDone && !m_vFiles.empty())
	{
		bool bResult = false;

		pBitmap = CreateOutputMemoryBitmap();
		if (static_cast<bool>(pBitmap))
			bResult = pBitmap->Init(m_lWidth, m_lHeight);

		if (m_bHomogenization)
		{
			// Create the bitmap that will contain the standard deviation/average for each pixel
			// to remove remaining star trails from the comet only image
			if (pBitmap->IsMonochrome())
				m_pHomBitmap = std::make_shared<C32BitFloatGrayBitmap>();
			else
				m_pHomBitmap = std::make_shared<C96BitFloatColorBitmap>();
			if (static_cast<bool>(m_pHomBitmap))
				bResult = m_pHomBitmap->Init(m_lWidth, m_lHeight);
		}

		if (pProgress != nullptr && bResult)
			pProgress->Start2(m_lHeight);

		const size_t lScanLineSize = static_cast<size_t>(m_lWidth) * GetNrBytesPerChannel() * GetNrChannels();
		std::vector<std::uint8_t> buffer;

		// Note:
		// Making the file reading concurrent (e.g. with std::async) is hardly a speed improvement,
		// because only about 7% of the time is spent for reading the data from the files.

		for (const auto& file : m_vFiles)
		{
			if (!bResult)
				break;

			// Read the full bitmap in memory
			const size_t fileSize = lScanLineSize * m_lNrAddedBitmaps * (size_t{ 1 } + file.m_lEndRow - file.m_lStartRow);

			if (fileSize > buffer.size())
				buffer.resize(fileSize);

			FILE* hFile = _tfopen(file.m_strFile, _T("rb"));
			if (hFile != nullptr)
			{
				bResult = fread(buffer.data(), 1, fileSize, hFile) == fileSize;
				fclose(hFile);
			}
			else
				bResult = false;

			if (!bResult)
				break;

			// More than 90% of the time of GetResult() is spent in CombineTask::process().
			// Only 7% for reading the data from files.
			CCombineTask{ file.m_lStartRow, file.m_lEndRow, lScanLineSize, buffer.data(), pProgress, this, pBitmap.get() }.process();

			if (pProgress != nullptr)
			{
				pProgress->End2();
				bResult = !pProgress->IsCanceled();
			}
		}

		if (bResult && static_cast<bool>(m_pHomBitmap))
		{
			// At this point the m_pHomBitmap might be used to smooth out any remaining
			// star trails with a large filter
			return SmoothOut(pBitmap.get(), pProgress);
		}
	}
	DestroyTempFiles();
	return pBitmap;
}
