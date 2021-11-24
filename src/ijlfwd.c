#include "ijlfwd.h"

HMODULE                 hIJL15Module = NULL;
PFNIJLGETLIBVERSIONPROC pIJLGetLibVersion = NULL;
PFNIJLINITPROC          pIJLInit = NULL;
PFNIJLFREEPROC          pIJLFree = NULL;
PFNIJLREADPROC          pIJLRead = NULL;
PFNIJLWRITEPROC         pIJLWrite = NULL;
PFNIJLERRORSTRPROC      pIJLErrorStr = NULL;


//lib version
int STDCALL ijlGetLibVersion() {
	int result = pIJLGetLibVersion();
	Log("test : %d", result);

	IJLibVersion LibVersion = { 1, 5, 4, "Luismk IJL 15", "1.5.4", "1.5.4", "May 24 2018", "Microsoft*" };
	return LibVersion.build;
}

int STDCALL ijlInit(int a) { // jcprops is JPEG_CORE_PROPERTIES*
	// TODO: Verify all this actually works
	return pIJLInit(a);
}

int STDCALL ijlFree(int a) { // jcprops is JPEG_CORE_PROPERTIES*
	// TODO: Forward the call
	// TODO: Unload base library
	return IJL_OK;
}

int STDCALL ijlRead(int a, int b) { // jcprops is JPEG_CORE_PROPERTIES*

										   // TODO: Forward the call
	int result = pIJLRead(a, b);
	Log("IJL15.READ: %d ", result);
	return pIJLRead(a, b);
}

int STDCALL ijlWrite(int a, int b) { // jcprops is JPEG_CORE_PROPERTIES*
	// TODO
	return pIJLWrite(a, b);
}
const char* STDCALL ijlErrorStr(int err) {
	// TODO: Forward the cal
	return pIJLErrorStr(err);
}


VOID InitIJL15() {
	if (hIJL15Module != NULL) {
		FatalError("Error: ijl15 is already initialized!");
	}

	hIJL15Module = LoadLib("ijl15_real");
	pIJLGetLibVersion = (PFNIJLGETLIBVERSIONPROC)GetProc(hIJL15Module, "ijlGetLibVersion");
	pIJLInit = (PFNIJLINITPROC)GetProc(hIJL15Module, "ijlInit");
	pIJLFree = (PFNIJLFREEPROC)GetProc(hIJL15Module, "ijlFree");
	pIJLRead = (PFNIJLREADPROC)GetProc(hIJL15Module, "ijlRead");
	pIJLWrite = (PFNIJLWRITEPROC)GetProc(hIJL15Module, "ijlWrite");
	pIJLErrorStr = (PFNIJLERRORSTRPROC)GetProc(hIJL15Module, "ijlErrorStr");
}

BOOL DecodeFromJPEGBuffer(
	BYTE* lpJpgBuffer,
	DWORD dwJpgBufferSize,
	BYTE** lppRgbBuffer,
	DWORD* lpdwWidth,
	DWORD* lpdwHeight,
	DWORD* lpdwNumberOfChannels) 
{

	BOOL bres;
	IJLERR jerr;
	DWORD dwWholeImageSize;
	BYTE* lpTemp = NULL;
	// Allocate the IJL JPEG_CORE_PROPERTIES structure.
	_JPEG_CORE_PROPERTIES jcprops = {0, 0};
	bres = TRUE;
	__try
	{
		// Initialize the Intel(R) JPEG Library.
		jerr = ijlInit(0);
		if (IJL_OK != jerr)
		{
			bres = FALSE;
			__leave;
		}
		// Get information on the JPEG image
		// (i.e., width, height, and channels).
		jcprops.JPGFile = NULL;
		jcprops.JPGBytes = lpJpgBuffer;
		jcprops.JPGSizeBytes = dwJpgBufferSize;
		jerr = ijlRead(0, IJL_JBUFF_READPARAMS);
		if (IJL_OK != jerr)
		{
			bres = FALSE;
			__leave;
		}
		// Set the JPG color space ... this will always be
		// somewhat of an educated guess at best because JPEG
		// is "color blind" (i.e., nothing in the bit stream
		// tells you what color space the data was encoded from).
		// However, in this example we assume that we are
		// reading JFIF files which means that 3 channel images
		// are in the YCbCr color space and 1 channel images are
		// in the Y color space.
		switch (jcprops.JPGChannels)
		{
		case 1:
		{
			jcprops.JPGColor = IJL_G;
			jcprops.DIBColor = IJL_RGB;
			jcprops.DIBChannels = 3;
			break;
		}
		case 3:
		{
			jcprops.JPGColor = IJL_YCBCR;
			jcprops.DIBColor = IJL_RGB;
			jcprops.DIBChannels = 3;
			break;
		}
		default:
		{
			// This catches everything else, but no
			// color twist will be performed by the IJL.
			jcprops.JPGColor = 255;
			jcprops.DIBColor = 255;
			jcprops.DIBChannels = jcprops.JPGChannels;
			break;
		}
		}
		// Compute size of desired pixel buffer.
		dwWholeImageSize = jcprops.JPGWidth * jcprops.JPGHeight *
			jcprops.DIBChannels;
		// Allocate memory to hold the decompressed image data.

		//memset(lpTemp,0, dwWholeImageSize);
		if (NULL == lpTemp)
		{
			bres = FALSE;
			__leave;
		}
		// Set up the info on the desired DIB properties.
		jcprops.DIBWidth = jcprops.JPGWidth;
		jcprops.DIBHeight = jcprops.JPGHeight;
		jcprops.DIBPadBytes = 0;
		jcprops.DIBBytes = lpTemp;
		// Now get the actual JPEG image data into the pixel buffer.
		jerr = ijlRead(0, IJL_JBUFF_READWHOLEIMAGE);
		if (IJL_OK != jerr)
		{
			bres = FALSE;
			__leave;
		}
	} // __try
	__finally
	{
		if (FALSE == bres)
		{
			if (NULL != lpTemp)
			{
				//free(lpTemp);
				lpTemp = NULL;
			}
		}
		// Clean up the Intel(R) JPEG Library.
		ijlFree(&jcprops);
		*lpdwWidth = jcprops.DIBWidth;
		*lpdwHeight = jcprops.DIBHeight;
		*lpdwNumberOfChannels = jcprops.DIBChannels;
		*lppRgbBuffer = lpTemp;
	} // __finally

	return bres;
}