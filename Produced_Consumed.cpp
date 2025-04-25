#include <windows.h>
#include <process.h>
#include <iostream>
#include <vector>
#include <stdexcept>  
#if __cplusplus < 201103L
    #define nullptr NULL  // Fallback for older C++ versions
#endif

using namespace std;

class Buffer {
private:
    static const int BUFFER_SIZE = 100;
    vector<int> buffer;
    int in, out;
    bool isBufferFull;
    bool running;
    
    HANDLE mutex;
    HANDLE emptySemaphore;
    HANDLE fullSemaphore;

public:
    Buffer() : buffer(BUFFER_SIZE), in(0), out(0), isBufferFull(false), running(true) {
        // Initialize synchronization objects with error checking
        mutex = CreateMutex(NULL, FALSE, NULL);
        if (mutex == NULL) {
            throw runtime_error("Failed to create mutex");
        }

        emptySemaphore = CreateSemaphore(NULL, BUFFER_SIZE, BUFFER_SIZE, NULL);
        if (emptySemaphore == NULL) {
            CloseHandle(mutex);
            throw runtime_error("Failed to create empty semaphore");
        }

        fullSemaphore = CreateSemaphore(NULL, 0, BUFFER_SIZE, NULL);
        if (fullSemaphore == NULL) {
            CloseHandle(mutex);
            CloseHandle(emptySemaphore);
            throw runtime_error("Failed to create full semaphore");
        }
    }

    ~Buffer() {
        CloseHandle(mutex);
        CloseHandle(emptySemaphore);
        CloseHandle(fullSemaphore);
    }

    void showBuffer() {
        cout << "\nCurrent Buffer: ";
        for (int i = 0; i < BUFFER_SIZE; i++) {
            cout << buffer[i] << " ";
        }
        cout << endl;
    }

    bool isRunning() const { return running; }
    void stop() { running = false; }

    void produce() {
        static int nextProduced = 1;
        cout << "\nProduced: ";
        
        while (running) {
            if (WaitForSingleObject(emptySemaphore, 0) == WAIT_OBJECT_0) {
                if (WaitForSingleObject(mutex, INFINITE) == WAIT_FAILED) {
                    cerr << "Error waiting for mutex" << endl;
                    return;
                }
                
                buffer[in] = nextProduced++;
                cout << buffer[in] << " ";
                in = (in + 1) % BUFFER_SIZE;
                isBufferFull = false;

                ReleaseSemaphore(fullSemaphore, 1, NULL);
                ReleaseMutex(mutex);
            } else {
                if (!isBufferFull) {
                    showBuffer();
                    cout << "\nBuffer is full\n";
                    cout << "\nEnter the number of items to consume (0 to exit): ";
                    isBufferFull = true;
                }
            }
            Sleep(100);
        }
        cout << endl;
    }

    void consume(int count) {
        if (count == 0) {
            running = false;
            return;
        }
cout << "Consumed: ";
        for (int i = 0; i < count && running; i++) {
            if (WaitForSingleObject(fullSemaphore, 0) == WAIT_OBJECT_0) {
                if (WaitForSingleObject(mutex, INFINITE) == WAIT_FAILED) {
                    cerr << "Error waiting for mutex" << endl;
                    return;
                }

                int nextConsumed = buffer[out];
                cout << nextConsumed << " ";
                out = (out + 1) % BUFFER_SIZE;

                ReleaseSemaphore(emptySemaphore, 1, NULL);
                ReleaseMutex(mutex);
            } else {
                cout << "\nBuffer is empty. Cannot consume.\n";
                break;
            }
        }
    }
};

Buffer* sharedBuffer = nullptr;

unsigned __stdcall Producer(void*) {
    try {
        sharedBuffer->produce();
    } catch (const exception& e) {
        cerr << "Producer error: " << e.what() << endl;
    }
    return 0;
}

unsigned __stdcall Consumer(void*) {
    try {
        while (sharedBuffer->isRunning()) {
            int consumeCount;
            cin >> consumeCount;
            sharedBuffer->consume(consumeCount);
        }
    } catch (const exception& e) {
        cerr << "Consumer error: " << e.what() << endl;
    }
    return 0;
}

int main() {
    try {
        sharedBuffer = new Buffer();
        sharedBuffer->showBuffer();

        HANDLE hThreads[2];
        hThreads[0] = (HANDLE)_beginthreadex(0, 0, Producer, NULL, 0, 0);
        if (hThreads[0] == 0) {
            throw runtime_error("Failed to create producer thread");
        }

        hThreads[1] = (HANDLE)_beginthreadex(0, 0, Consumer, NULL, 0, 0);
        if (hThreads[1] == 0) {
            CloseHandle(hThreads[0]);
            throw runtime_error("Failed to create consumer thread");
        }

        WaitForMultipleObjects(2, hThreads, TRUE, INFINITE);

        CloseHandle(hThreads[0]);
        CloseHandle(hThreads[1]);
        delete sharedBuffer;
        
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}
