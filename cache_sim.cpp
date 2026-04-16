#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <memory>
#include <limits> 
#include <unordered_map> // Added for Main Memory Simulation

using namespace std;

// ==========================================
// 1. ARCHITECTURE DEFINITIONS
// ==========================================
const int CACHE_SIZE_KB = 16;
const int BLOCK_SIZE_BYTES = 16; 
const int NUM_BLOCKS = (CACHE_SIZE_KB * 1024) / BLOCK_SIZE_BYTES; 

const int MEMORY_LATENCY = 100; // Fixed memory latency restored
const int CACHE_LATENCY = 1;  

// CPU Request Structure
struct Request {
    string type;     
    uint32_t address;
    uint32_t data;   
};

// Cache Block with Exact Bit-Fields
struct CacheBlock {
    uint32_t tag : 18;    
    uint32_t valid : 1;   
    uint32_t dirty : 1;   
    uint32_t data[4]; // 4 words (16 bytes) of payload     

    CacheBlock() : tag(0), valid(0), dirty(0) {
        for(int i = 0; i < 4; ++i) data[i] = 0;
    }
};

// ==========================================
// 2. FORWARD DECLARATIONS FOR STATE PATTERN
// ==========================================
class CacheController;

class CacheState {
public:
    virtual void handle(CacheController* context) = 0;
    virtual string getName() const = 0;
    virtual ~CacheState() = default;
};

// ==========================================
// 3. CACHE CONTROLLER (CONTEXT CLASS)
// ==========================================
class CacheController {
private:
    CacheState* currentState;
    vector<CacheBlock> cache;
    
public:
    Request currentReq;
    int memoryWaitCounter;
    int cacheWaitCounter;
    bool requestCompleted;
    int currentCycle;
    
    // Simulating Main Memory (Address -> 32-bit Word)
    unordered_map<uint32_t, uint32_t> mainMemory;

    CacheController();

    void changeState(CacheState* newState) {
        currentState = newState;
    }

    void process(Request req) {
        currentReq = req;
        requestCompleted = false;
        currentCycle = 1;

        // Force address to be 32-bit word aligned (multiple of 4)
        currentReq.address = currentReq.address & ~0x3;

        cout << "\n=================================================" << endl;
        cout << "Processing " << req.type << " at Address: 0x" << hex << uppercase << req.address << dec << endl;
        
        while (!requestCompleted) {
            cout << "Cycle " << setw(3) << currentCycle << " | State: " << setw(12) << left << currentState->getName() << " | ";
            currentState->handle(this);
            currentCycle++;
        }
    }

    // Helper functions for 32-bit address decoding
    uint32_t getWordOffset() const { return (currentReq.address & 0xF) >> 2; } // Bits [3:2] gives index 0-3
    uint32_t getIndex() const  { return (currentReq.address >> 4) & 0x3FF; }  
    uint32_t getTag() const    { return (currentReq.address >> 14) & 0x3FFFF; } 

    CacheBlock& getBlock(uint32_t index) { return cache[index]; }
};

// ==========================================
// 4. CONCRETE STATES (SINGLETONS)
// ==========================================

class IdleState : public CacheState {
public:
    static IdleState& getInstance() { static IdleState instance; return instance; }
    string getName() const override { return "IDLE"; }
    void handle(CacheController* ctx) override;
};

class CompareTagState : public CacheState {
public:
    static CompareTagState& getInstance() { static CompareTagState instance; return instance; }
    string getName() const override { return "COMPARE_TAG"; }
    void handle(CacheController* ctx) override;
};

class WriteDataState : public CacheState {
public:
    static WriteDataState& getInstance() { static WriteDataState instance; return instance; }
    string getName() const override { return "WRITE_DATA"; }
    void handle(CacheController* ctx) override;
};

class WriteBackState : public CacheState {
public:
    static WriteBackState& getInstance() { static WriteBackState instance; return instance; }
    string getName() const override { return "WRITE_BACK"; }
    void handle(CacheController* ctx) override;
};

class AllocateState : public CacheState {
public:
    static AllocateState& getInstance() { static AllocateState instance; return instance; }
    string getName() const override { return "ALLOCATE"; }
    void handle(CacheController* ctx) override;
};

// ==========================================
// 5. STATE TRANSITION LOGIC IMPL
// ==========================================

CacheController::CacheController() {
    cache.resize(NUM_BLOCKS);
    currentState = &IdleState::getInstance();
    memoryWaitCounter = 0;
    cacheWaitCounter = 0;
}

void IdleState::handle(CacheController* ctx) {
    cout << "Valid request detected. Latching and moving to COMPARE_TAG." << endl;
    ctx->cacheWaitCounter = 0;
    ctx->changeState(&CompareTagState::getInstance());
}

void CompareTagState::handle(CacheController* ctx) {
    uint32_t index = ctx->getIndex();
    uint32_t tag = ctx->getTag();
    CacheBlock& block = ctx->getBlock(index);

    if (ctx->cacheWaitCounter < CACHE_LATENCY - 1) {
        cout << "Searching cache array... (" << ctx->cacheWaitCounter + 1 << "/" << CACHE_LATENCY << ")" << endl;
        ctx->cacheWaitCounter++;
        return;
    }

    if (block.valid == 1 && block.tag == tag) {
        cout << "CACHE HIT! ";
        if (ctx->currentReq.type == "WRITE") {
            cout << "Tag matched. Transitioning to WRITE_DATA state." << endl;
            ctx->changeState(&WriteDataState::getInstance());
        } else {
            // Retrieve actual data based on the word offset
            uint32_t wordOffset = ctx->getWordOffset();
            cout << "Reading data [" << dec << block.data[wordOffset] << "] to CPU." << endl;
            ctx->requestCompleted = true;
            ctx->changeState(&IdleState::getInstance());
        }
    } else {
        cout << "CACHE MISS. ";
        if (block.valid == 1 && block.dirty == 1) {
            cout << "Conflict: Evicted block is DIRTY. Transitioning to WRITE_BACK." << endl;
            ctx->memoryWaitCounter = 0;
            ctx->changeState(&WriteBackState::getInstance());
        } else {
            cout << "Conflict/Compulsory: Block is CLEAN. Transitioning to ALLOCATE." << endl;
            ctx->memoryWaitCounter = 0;
            ctx->changeState(&AllocateState::getInstance());
        }
    }
}

void WriteDataState::handle(CacheController* ctx) {
    CacheBlock& block = ctx->getBlock(ctx->getIndex());
    uint32_t wordOffset = ctx->getWordOffset();
    
    // Store data in the specific word of the cache block
    block.data[wordOffset] = ctx->currentReq.data;
    block.dirty = 1; 
    
    cout << "Writing payload [" << dec << ctx->currentReq.data << "] to block word " << wordOffset << ". Dirty bit set to 1. Request Complete." << endl;
    
    ctx->requestCompleted = true;
    ctx->changeState(&IdleState::getInstance());
}

void WriteBackState::handle(CacheController* ctx) {
    if (ctx->memoryWaitCounter < MEMORY_LATENCY - 1) {
        cout << "Writing dirty block to RAM... (" << ctx->memoryWaitCounter + 1 << "/" << MEMORY_LATENCY << ")" << endl;
        ctx->memoryWaitCounter++;
    } else {
        CacheBlock& block = ctx->getBlock(ctx->getIndex());
        
        // Reconstruct the base address of the evicted block
        uint32_t evictedBlockBaseAddr = (block.tag << 14) | (ctx->getIndex() << 4);
        
        // Write all 4 words back to Main Memory
        for(int i = 0; i < 4; i++) {
            ctx->mainMemory[evictedBlockBaseAddr + (i * 4)] = block.data[i];
        }

        cout << "RAM Write Complete (" << MEMORY_LATENCY << "/" << MEMORY_LATENCY << "). Dirty block safely in memory. Moving to ALLOCATE." << endl;
        block.dirty = 0; 
        ctx->memoryWaitCounter = 0;
        ctx->changeState(&AllocateState::getInstance());
    }
}

void AllocateState::handle(CacheController* ctx) {
    if (ctx->memoryWaitCounter < MEMORY_LATENCY - 1) {
        cout << "Fetching new block from RAM... (" << ctx->memoryWaitCounter + 1 << "/" << MEMORY_LATENCY << ")" << endl;
        ctx->memoryWaitCounter++;
    } else {
        uint32_t index = ctx->getIndex();
        uint32_t tag = ctx->getTag();
        CacheBlock& block = ctx->getBlock(index);
        
        // Calculate the base address of the newly requested block
        uint32_t newBlockBaseAddr = ctx->currentReq.address & ~0xF; 
        
        // Fetch all 4 words from Main Memory into the Cache Block
        for(int i = 0; i < 4; i++) {
            block.data[i] = ctx->mainMemory[newBlockBaseAddr + (i * 4)];
        }
        
        cout << "RAM Read Complete (" << MEMORY_LATENCY << "/" << MEMORY_LATENCY << "). Block installed. Returning to COMPARE_TAG." << endl;
        
        block.valid = 1;
        block.tag = tag;
        
        ctx->memoryWaitCounter = 0;
        ctx->cacheWaitCounter = 0;
        ctx->changeState(&CompareTagState::getInstance());
    }
}

// ==========================================
// 6. MAIN EXECUTION & TESTING
// ==========================================
int main() {
    cout << R"(
 _   _                        ____                        
| \ | | _____   _____ _ __   / ___| ___  _ __  _ __   __ _ 
|  \| |/ _ \ \ / / _ \  __| | |  _ / _ \|  _ \|  _ \ / _  |
| |\  |  __/\ V /  __/ |    | |_| | (_) | | | | | | | (_| |
|_| \_|\___| \_/ \___|_|     \____|\___/|_| |_|_| |_|\__,_|
  ____ _                __   __          _   _       
 / ___(_)_   _____      \ \ / /__  _   _| | | | _ __ 
| |  _| \ \ / / _ \ _____\ V / _ \| | | | | | ||  _ \
| |_| | |\ V /  __/ _____ | | (_) | |_| | |_| || |_) |
 \____|_| \_/ \___|       |_|\___/ \__,_|\___/ |  __/ 
                                               |_|    
    )" << '\n';
    
    CacheController controller;
    int modeChoice;

    cout << "=================================================\n";
    cout << "   FSM Cache Controller Simulator (16 KiB)\n";
    cout << "=================================================\n";
    cout << "Select Operating Mode:\n";
    cout << " [1] Run Automated Test Suite (All Scenarios)\n";
    cout << " [2] Interactive CPU Mode\n";
    cout << "Choice: ";
    
    if (!(cin >> modeChoice)) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        modeChoice = 2; // Default to interactive on bad input
    }

    if (modeChoice == 1) {
        cout << "\n=================================================";
        cout << "\n   STARTING AUTOMATED SIMULATION";
        cout << "\n=================================================\n";
        
        cout << "\n--> SCENARIO 1: Compulsory Miss (Clean Allocate)";
        controller.process({"READ", 0x1000, 0});
        
        cout << "\n--> SCENARIO 2: Write Hit (Sets Dirty Bit)";
        controller.process({"WRITE", 0x1000, 42});
        
        cout << "\n--> SCENARIO 3: Spatial Locality Hit (Different Word, Same Block)";
        controller.process({"READ", 0x1004, 0});
        
        cout << "\n--> SCENARIO 4: Compulsory Miss #2";
        controller.process({"READ", 0xA000, 0});
        
        cout << "\n--> SCENARIO 5: Conflict Miss (Evicts Clean Block 0xA000)";
        controller.process({"READ", 0xE000, 0});
        
        cout << "\n--> SCENARIO 6: Conflict Miss (Evicts Dirty Block 0x1000 -> Triggers Write-Back)";
        controller.process({"WRITE", 0x5000, 99});

        cout << "\n=================================================";
        cout << "\n   AUTOMATED SIMULATION COMPLETE";
        cout << "\n=================================================\n";
        return 0;
    }

    // Interactive Mode
    string command;
    uint32_t address;
    uint32_t data;

    cout << "\n=================================================\n";
    cout << "Instructions:\n";
    cout << " Act as the CPU by entering memory requests.\n";
    cout << " \n";
    cout << " Format for READ:  READ <hex_address>\n";
    cout << " Format for WRITE: WRITE <hex_address> <integer_data>\n";
    cout << " Type EXIT to quit.\n";
    cout << "=================================================\n\n";

    while (true) {
        cout << "\nCPU> ";
        cin >> command;

        for (auto &c : command) c = toupper(c);

        if (command == "EXIT") {
            cout << "Shutting down cache controller..." << endl;
            break;
        }

        if (command == "READ") {
            if (cin >> hex >> address) {
                controller.process({"READ", address, 0});
            } else {
                cout << "[Error] Invalid address format." << endl;
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
            }
        } else if (command == "WRITE") {
            if (cin >> hex >> address >> dec >> data) {
                controller.process({"WRITE", address, data});
            } else {
                cout << "[Error] Invalid input format. Expected: WRITE <hex_addr> <dec_data>" << endl;
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
            }
        } else {
            cout << "[Error] Unknown command. Please use READ, WRITE, or EXIT." << endl;
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }
    }

    return 0;
}