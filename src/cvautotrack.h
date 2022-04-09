#include <string>
typedef bool (*cvInitProc)();
typedef bool (*cvUnInitProc)();
typedef bool (*cvSetHandleProc)(long long int handle);
typedef bool (*cvSetWorldCenterProc)(double x, double y);
typedef bool (*cvSetWorldScaleProc)(double scale);
typedef bool (*cvGetTransformProc)(float &x, float &y, float &a);
typedef bool (*cvGetPositionProc)(double &x, double &y);
typedef bool (*cvGetDirectionProc)(double &a);
typedef bool (*cvGetRotationProc)(double &a);
typedef int (*cvGetLastErrProc)();

cvInitProc cvInit;
cvUnInitProc cvUnInit;
cvSetHandleProc cvSetHandle;
cvSetWorldCenterProc cvSetWorldCenter;
cvSetWorldScaleProc cvSetWorldScale;
cvGetTransformProc cvGetTransform;
cvGetPositionProc cvGetPosition;
cvGetDirectionProc cvGetDirection;
cvGetRotationProc cvGetRotation;
cvGetLastErrProc cvGetLastErr;

bool init();
bool uninit();
bool SetHandle(long long int handle);
bool SetWorldCenter(double x, double y);
bool SetWorldScale(double scale);
bool GetTransform(float &x, float &y, float &a);
bool GetPosition(double &x, double &y);
bool GetDirection(double &a);
bool GetRotation(double &a);
int GetLastErr();
std::string load();
std::string updateDLL();
std::string getDLLUpdateTime();
bool track(double &x, double &y, double &a, double &b);
bool startTrack();
bool stopTrack();