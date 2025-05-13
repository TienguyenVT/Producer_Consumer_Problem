#include <windows.h>
#include <process.h>
#include <iostream>
#include <vector>
#include <stdexcept>  
#if __cplusplus < 201103L
    #define nullptr NULL 
#endif

using namespace std;

class Buffer {
private:
    static const int BUFFER_SIZE = 20;
    vector<int> buffer;
    int count; 
    bool running;
    int remainingToConsume;  
    bool waitingForMore;     
    
    HANDLE mutex;
    HANDLE emptySemaphore;
    HANDLE fullSemaphore;

public:
    Buffer() : buffer(BUFFER_SIZE, 0), count(0), running(true), 
               remainingToConsume(0), waitingForMore(false) {
        
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
        cout << "\nCurrent Buffer (" << count << " elements): ";
        for (int i = 0; i < BUFFER_SIZE; i++) {
            if (i < count) {
                cout << buffer[i] << " ";
            } else {
                cout << "_ ";  
            }
        }
        cout << endl;
    }

    bool isRunning() const { return running; }
    void stop() { running = false; }

    void produce() {
        static int nextProduced = 1;
        static bool messageShown = false;  
        cout << "\nProduced: ";
        
        while (running) {
            if (WaitForSingleObject(emptySemaphore, 0) == WAIT_OBJECT_0) {
                messageShown = false;  
                
                if (WaitForSingleObject(mutex, INFINITE) == WAIT_FAILED) {
                    cerr << "Error waiting for mutex" << endl;
                    return;
                }
                
                buffer[count] = nextProduced++;
                cout << buffer[count] << " ";
                count++;

               
                if (count == BUFFER_SIZE && waitingForMore) {
                    cout << "\nBuffer is full. Auto-consuming remaining items...\n";
                    ReleaseMutex(mutex);
                    consume(remainingToConsume);
                } else {
                    ReleaseSemaphore(fullSemaphore, 1, NULL);
                    ReleaseMutex(mutex);
                }
            } else {
                if (count >= BUFFER_SIZE && !messageShown) { 
                    showBuffer();
                    cout << "\nBuffer is full\n";
                    if (waitingForMore) {
                        cout << "Waiting to consume remaining " << remainingToConsume << " items\n";
                    }
                    cout << "\nEnter the number of items to consume (0 to exit): ";
                    messageShown = true;  
                }
            }
            Sleep(100);
        }
        cout << endl;
    }

    void consume(int consumeCount) {
        if (consumeCount == 0) {
            running = false;
            return;
        }

        cout << "Consumed: ";
        if (WaitForSingleObject(mutex, INFINITE) == WAIT_FAILED) {
            cerr << "Error waiting for mutex" << endl;
            return;
        }

 
        int availableItems = min(consumeCount, count);
        
        if (availableItems == 0) {
            cout << "\nBuffer is empty. Cannot consume.\n";
            ReleaseMutex(mutex);
            return;
        }


        for (int i = 0; i < availableItems; i++) {
            cout << buffer[i] << " ";
        }
        cout << endl;

       
        for (int i = 0; i < count - availableItems; i++) {
            buffer[i] = buffer[i + availableItems];
        }


        count -= availableItems;

        remainingToConsume = consumeCount - availableItems;
        if (remainingToConsume > 0) {
            waitingForMore = true;
            cout << "\nBuffer is empty. Will auto-consume remaining " 
                 << remainingToConsume << " items when buffer is full again.\n";
        } else {
            waitingForMore = false;
        }


        ReleaseSemaphore(emptySemaphore, availableItems, NULL);

        showBuffer();
        
        ReleaseMutex(mutex);
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
