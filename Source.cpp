#include <iostream>
#include <thread>
#include <chrono>        
#include <limits>
#include <sstream>
#include <vector>
#include <algorithm>    // std::min
#include <assert.h>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <WinDef.h>
#elif defined(X11)
#error x11 todo..

#else
#error NOT PORTED TO THIS FKING OS YET.. (PS: using WinAPI all over the place :( )
#endif
#define TARGET_TITLE_MAX_LENGTH 0xFFFF //TODO: use GetWindowTextLength --and it seems bugged, sometimes returning a few bytes less than needed :s

struct BitmapDif {
	uint64_t bytesDifferent;
	double  percentageDif;
	uint64_t image1Size;
	uint64_t image2Size;
};

void getTarget(HWND* targetHWND, char* targetTitle){
	//GetPhysicalCursorPos
	//GetAsyncKeyState(VK_LBUTTON)
	//    if ((GetKeyState(VK_RBUTTON) & 0x80) != 0)'
	// GetAsyncKeyState
	GetAsyncKeyState(VK_RBUTTON);
	POINT pt;
	while (0 == static_cast<unsigned char>(GetAsyncKeyState(VK_RBUTTON))){
		std::this_thread::sleep_for(std::chrono::milliseconds(50));//gotta be fast to avoid user moving over something else...
		//and its still not fool-proof, there's *probably* a better, event-driven way to do this, buuuuuuut i don't know how. 
	//maybe some kind of keyboard driver hook
	// ... like http://msdn.microsoft.com/en-us/library/windows/desktop/ms644985(v=vs.85).aspx / LowLevelKeyboardProc callback :)
	}
	assert(GetPhysicalCursorPos(&pt)!=false);
	(*targetHWND) = WindowFromPoint(pt);
	memset(targetTitle, 0x00, TARGET_TITLE_MAX_LENGTH + 1);
	GetWindowText(*targetHWND, targetTitle, TARGET_TITLE_MAX_LENGTH);
	std::cout << "HWND id: \"" << (*targetHWND) << "\"\ntitle: \"" << targetTitle << "\"" << std::endl;
}




std::vector<unsigned char> GetBytes(const HBITMAP BitmapHandle){
	BITMAP Bmp = { 0 };//WARNING: not accurate!!! doesnt account for header, i think :s probably missing a few bytes. should fix this sometime
	GetObject(BitmapHandle, sizeof(Bmp), &Bmp);
	size_t length = (((Bmp.bmWidth * Bmp.bmHeight) * Bmp.bmBitsPixel) / 8);// +sizeof(BITMAPINFOHEADER) ?
	std::vector <unsigned char> ret;

	ret.resize(length);
	if (ret.size() == 0){ return ret; }
	GetBitmapBits(BitmapHandle, length, &ret[0]);

	//	memcpy(&ret[0], Bmp.bmBits, length);
	return ret;
}

std::vector<unsigned char> captureImage2(HWND* target){
	// thank you http://stackoverflow.com/a/7292773
	RECT rc;
	HBITMAP hbmp;
	GetClientRect(*target, &rc);
	HDC hdcScreen = GetDC(NULL);
	HDC hdc = CreateCompatibleDC(hdcScreen);
	hbmp = CreateCompatibleBitmap(hdcScreen,
		rc.right - rc.left, rc.bottom - rc.top);
	SelectObject(hdc, hbmp);

	//Print to memory hdc
	PrintWindow(*target, hdc, PW_CLIENTONLY);
#if defined(_DEBUG )
	//	//copy to clipboard
	OpenClipboard(NULL);
	EmptyClipboard();
	SetClipboardData(CF_BITMAP, hbmp);
	CloseClipboard();
#endif//_DEBUG 
	auto ret = GetBytes(hbmp);//
	//release
	DeleteDC(hdc);
	DeleteObject(hbmp);
	ReleaseDC(NULL, hdcScreen);
	return ret;
}

void compareBitmaps2(const std::vector<unsigned char> img1, const std::vector<unsigned char> img2, BitmapDif* result)
{
	int64_t bytesDifferent = 0;
	if (img1.size() == 0){
		bytesDifferent = img2.size();
	}
	else
		if (img2.size() == 0){
		bytesDifferent = img1.size();
		}
		else {
			const unsigned char* img1Bytes = &img1[0];
			const unsigned char* img2Bytes = &img2[0];
			const int64_t maxlen = std::min(img1.size(), img2.size());
			bytesDifferent += (std::max(img1.size(), img2.size()) - maxlen);
			for (int64_t i = 0; i < maxlen; ++i){
				if (img1Bytes[i] != img2Bytes[i]){
					++bytesDifferent;
				}
			}
		}
		result->bytesDifferent = bytesDifferent;
		result->image1Size = img1.size();
		result->image2Size = img2.size();
		//(*result).percentageDif = ((double(img1DataVector.size()) - double(bytesDifferent)) / double(img1DataVector.size())) * double(100);
		int foo = img1.size();
		if (foo == 0)foo = 1;
		result->percentageDif = 100.0 - (((img1.size() - bytesDifferent) * 100.0) / foo);
}

inline void beep(void){ std::cout << '\a';/*<< std::flush ?*/}
inline void clear(void){ system("cls"); }
inline void pause(void){
	//std::cin.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
	//std::cin.get();
	system("pause");//ffs.
}
volatile bool beepnow = false;
void beepforever(void){ while (1){ if(beepnow)beep(); std::this_thread::sleep_for(std::chrono::seconds(1));}}
int main()
{
	std::cout << "left-click 3 times (or something..) on your target window now..." << std::endl;
	HWND targetHWND = 0;
	//char* targetTitle = static_cast<char*>( calloc(1,TARGET_TITLE_MAX_LENGTH+1));
	char targetTitle[TARGET_TITLE_MAX_LENGTH + 1];
	getTarget(&targetHWND, targetTitle);
	std::vector<unsigned char> oldimage, newimage;
	std::cout << "how many seconds to sleep between checks?: ";
	int sleeptime = 0;
	while (!(std::cin >> sleeptime)){
		std::cin.clear();
		std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		std::cout << "Invalid input.  Try again: ";
	}
	std::cout << "checking every \"" << sleeptime << "\" seconds.." << std::endl;
	std::cout << "threshold in percent? ... (0-99 ? ): ";
	int64_t threshold = 0;
	while (!(std::cin >> threshold)){
		std::cin.clear();
		std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		std::cout << "Invalid input.  Try again: ";
	}

	std::cout << "threshold: \"" << threshold << "\" percent. Starting." << std::endl;
	BitmapDif diff;
	memset(&diff, 0x01, sizeof(decltype(diff)));
//#define MemoryLeak
#ifdef MemoryLeak
	uint64_t count = 0;
	while (1){
		++count;
		captureImage(&targetHWND, &newimage);
		std::cout << count << "\n";
	}
#endif

	while (true){
		oldimage = captureImage2(&targetHWND);
		std::this_thread::sleep_for(std::chrono::seconds(sleeptime));
		newimage = captureImage2(&targetHWND);
		compareBitmaps2(oldimage, newimage, &diff);
		std::cout << "(diff.bytesDifferent: " << diff.bytesDifferent << " diff.image1Size: " << diff.image1Size + 1 << " diff.image2Size: " << diff.image2Size << "  diff.percentageDif: " << diff.percentageDif /*static_cast<const int>(diff.percentageDif)*/ << ")"<< std::endl;
		if (diff.percentageDif <= threshold)
		{
			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 12);//<<red text..
			beepnow = true;
			std::cout << "Threshold reached!! press enter to stop beeping.." << std::flush;
			//std::thread beepthread([](void)->void{while (1){ beep(); std::this_thread::sleep_for(std::chrono::seconds(1)); } });
				static std::thread beepthread(beepforever);
				pause();
				beepnow = false;
				SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED);//<< sucks if this isn't the default. TODO: fix this shit to go back to old settings, whatever they were 
				std::cout << "press enter to continue.." << std::flush;
				pause();
				//			exit(0);
		}
	}
	return 0;
}

