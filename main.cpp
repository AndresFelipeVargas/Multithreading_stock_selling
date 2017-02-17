#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <thread>
#include <chrono>         // std::chrono::seconds
#include <vector>
#include <queue>
#include <mutex>
#include <stdlib.h>
#include <random>

class Stock{
private:
    std::string symbol;
    int numOfStocks;
    double avgPricePaid;
    std::queue<double> priceList;
public:
    // Stock lock
    std::mutex stockLock;

    Stock(){
        numOfStocks = 0;
        avgPricePaid = 0;
    }

    Stock(const Stock&){
        numOfStocks = 0;
        avgPricePaid = 0;
    }

    // Set functions
    void setSymbol(std::string symbol){
        this->symbol = symbol;
    }

    void setPriceList(std::queue<double> list){
        priceList = list;
    }

    // Get functions
    std::string getSymbol(){
        return symbol;
    }

    double getAvgPricePaid(){
        return avgPricePaid;
    }

    int getNumOfStock(){
        return numOfStocks;
    }

    double getNextPrice(){
        double val = priceList.front();
        priceList.pop();
        return val;
    }

    double viewNextPrice(){
        return priceList.front();
    }

    void updateShareInfo(int amountPurchased, double costPerShare){
        numOfStocks += amountPurchased;
        avgPricePaid = (avgPricePaid + costPerShare)/2;
    }

    // Stock was sold
    void stockSold(){
        avgPricePaid = 0;
        numOfStocks = 0;
    }
};

// Stores the info to sell the stock
struct sellStockInfo {
    std::string symbol;
    double currentPrice;
    double avgPricePaid;
    int totalStocks;
};

// Initialize all of the require values
const int NUMOFFINALTRANSACTIONS = 10000;
double balance = 500000;
double yield = 0;
double profit = 0;         // Revenue from sales
double tCost = 0;           // Cost from purchases
int numOfTransactions = 0;
int numOfDoneTrans = 0;
int activeTreads = 0;
int numOfSell = 0;
bool isBuy = true;
bool serverDone = false;
double shouldBuy = 60;     // Z variable in percentage
double sellIfOver = 5;     // x variable in percentage
double sellIfUnder = 5;    // y variable in percentage
int numOfStocksAvailable = 0;
std::mutex globalLock;
std::vector<Stock> stocksAvailable(10);
std::vector<std::string> currentStocks = {}; // Vector that keeps track of currently owned stocks
std::vector<sellStockInfo> sellStockVector = {}; // Vector that holds the stock sell info that is passed into the thread
// Random number generator used for threads
std::default_random_engine randomGenerator;
std::uniform_int_distribution<int> numOfStocks(1, 200);

void server(){
    while(true) {

        std::cout << std::endl << "Current balance: " << balance << "." << std::endl;
        std::cout << "Current profit: " << profit << "." << std::endl;
        std::cout << "Current cost: " << tCost << "." << std::endl;
        yield = profit - tCost;
        std::cout << "Amount yield: " << yield << "." << std::endl;
        std::cout << "Number of active threads: " << activeTreads << "." << std::endl;
        std::cout << "Number of done transactions: " << numOfDoneTrans << "." << std::endl << std::endl;

        // Stop printing when the last transaction is done
        if (numOfDoneTrans == NUMOFFINALTRANSACTIONS){
            serverDone = true;
            break;
        }

        // Wait for 5 seconds before displaying the data again
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void processTransaction(std::string transactionString){
    // Initialize all of the variables used
    std::istringstream iss(transactionString);
    std::string option;
    std::string stockSymbol;
    std::string numOfSharesStr;
    std::string pricePerShareStr;
    int numOfShares;
    double pricePerShare;
    double costOfTransaction;
    double profitOfTransaction;
    bool ownStock;
    Stock *selectedStock = NULL;


    // Grab the portions from the string
    iss >> option;
    iss >> stockSymbol;
    iss >> numOfSharesStr;
    iss >> pricePerShareStr;

    // Convert the strings to their respective format
    numOfShares = std::stoi(numOfSharesStr);
    pricePerShare = std::stod(pricePerShareStr);

    // Match the symbol to the appropriate stock
    for (int i = 0; i < numOfStocksAvailable; i++){
        if(stockSymbol == stocksAvailable[i].getSymbol()){
            selectedStock = &stocksAvailable[i];
            break;
        }
    }

    if(option == "buy"){
        // Update the  stock info
        selectedStock->stockLock.lock();
        selectedStock->updateShareInfo(numOfShares, pricePerShare);
        selectedStock->stockLock.unlock();

        // Update the cost and balance
        costOfTransaction = (numOfShares * pricePerShare);

        globalLock.lock();
        tCost += costOfTransaction;
        balance -= costOfTransaction;
        globalLock.unlock();

        ownStock = false;

        // Check if the stock is already owned. If it is not, then add it to the currentStocks vector
        for(int i = 0; i < currentStocks.size(); i++){
            if(stockSymbol == currentStocks[i]){
                ownStock = true;
            }
        }

        if(!ownStock) {
            globalLock.lock();
            currentStocks.push_back(stockSymbol);
            globalLock.unlock();
        }
    } else if(option == "sell"){
        // Update the profit and balance
        profitOfTransaction = (numOfShares * pricePerShare);
        globalLock.lock();
        profit += profitOfTransaction;
        balance += profitOfTransaction;
        globalLock.unlock();
    }

}

void buyStock(){
    globalLock.lock();
    activeTreads++;
    globalLock.unlock();

    //  Select random stock from the available stock
    std::uniform_int_distribution<int> stockDistribution(0, numOfStocksAvailable-1);
    int randomStockIndex = stockDistribution(randomGenerator);
    Stock *selectedStock = &stocksAvailable[randomStockIndex];
    int numToPurchase = numOfStocks(randomGenerator);;    // Purchase anywhere between 1-200 stocks

    selectedStock->stockLock.lock();
    double pricePerShare = selectedStock->getNextPrice();   // Grab the next available price for the stock
    selectedStock->stockLock.unlock();

    // Create the transaction string
    std::string transactionString = "buy ";
    transactionString += selectedStock->getSymbol() + " ";
    transactionString += std::to_string(numToPurchase) + " ";
    transactionString += std::to_string(pricePerShare);

    processTransaction(transactionString);

    std::this_thread::sleep_for(std::chrono::seconds(2)); // Sleep for 2 seconds to simulate the transaction being processed

    globalLock.lock();
    numOfDoneTrans++;
    activeTreads--;
    globalLock.unlock();

    return;
}

void sellStock(sellStockInfo stockInfo){
    globalLock.lock();
    activeTreads++;
    globalLock.unlock();

    int numToSell = stockInfo.totalStocks;

    std::string transactionString = "sell ";
    transactionString += stockInfo.symbol + " ";
    transactionString += std::to_string(numToSell) + " ";
    transactionString += std::to_string(stockInfo.currentPrice);

    if(stockInfo.symbol != "") {
        processTransaction(transactionString);
    }

    std::this_thread::sleep_for(std::chrono::seconds(2)); // Sleep for 2 seconds to simulate the transaction being processed

    globalLock.lock();
    numOfDoneTrans++;
    activeTreads--;
    globalLock.unlock();

    return;
}

void initializeStocks(){
    std::ifstream file ( "priceList.csv" );
    std::string value;

    std::vector<double> prices;
    while (getline ( file, value, ',' )) {  // This will get the value of the stock name
        std::queue<double> stockPrices;

        stocksAvailable[numOfStocksAvailable].setSymbol(value);

        getline ( file, value, ',' );
        while ( value != "\n@" && file.good()){   // This would loop though all of the stock prices
            stockPrices.push(stod(value));
            getline ( file, value, ',' );
        }

        stocksAvailable[numOfStocksAvailable].setPriceList(stockPrices);
        numOfStocksAvailable++;
    }
}


int main(){
    std::thread threads[NUMOFFINALTRANSACTIONS];

    // Initialize all of the stock info
    initializeStocks();

    // Create the server thread that will display the current info
    std::thread serverThread(server);
    serverThread.detach();

    // Randomize stocks based on the time
    std::srand(time(NULL));

    try {
        while (numOfTransactions < NUMOFFINALTRANSACTIONS) {

            // Limit thread creation to around 600 threads to avoid reaching resource limit.
            if(activeTreads > 400){
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            if (isBuy) {
                int probOfBuy = std::rand() % 101;  // Get a percentage between 0-100

                if (probOfBuy < shouldBuy) {

                    std::thread buyThread(buyStock);
                    buyThread.detach();
                    numOfTransactions++;

                }

                isBuy = false;  // Run the sell statement next

            } else {
                // Check if there are any stocks to sell. If not, then skip sell attempt
                if (currentStocks.empty()){
                    isBuy = true;
                    continue;
                }

                // Structures that will be used with the sell thread
                sellStockInfo stockInfo;
                Stock *selectedStock;
                int positionOfStock;

                int randomStockIndex = std::rand() % (currentStocks.size());
                stockInfo.symbol = currentStocks[randomStockIndex];

                // Find the stock based on the stock symbol
                for (int i = 0; i < numOfStocksAvailable; i++){
                    if(stockInfo.symbol == stocksAvailable[i].getSymbol()){
                        selectedStock = &stocksAvailable[i];
                        positionOfStock = i;
                        break;
                    }
                }

                stockInfo.currentPrice = (selectedStock->viewNextPrice());
                stockInfo.totalStocks = (selectedStock->getNumOfStock());
                stockInfo.avgPricePaid = (selectedStock->getAvgPricePaid());

                double adjustedPriceOver = (1 + ( ((double)sellIfOver) / 100.0) ) * stockInfo.avgPricePaid;
                double adjustedPriceUnder = (1 - ( ((double)sellIfUnder) / 100.0) ) * stockInfo.avgPricePaid;

                if(stockInfo.currentPrice > adjustedPriceOver or stockInfo.currentPrice < adjustedPriceUnder) {

                    // Remove stock so that it cannot be sold again
                    globalLock.lock();
                    currentStocks.erase(currentStocks.begin() + randomStockIndex);
                    globalLock.unlock();

                    //Remove the stock info
                    selectedStock->stockLock.lock();
                    stockInfo.currentPrice = (selectedStock->getNextPrice());
                    selectedStock->stockSold();
                    selectedStock->stockLock.unlock();

                    // Create a sell stock thread and increase the number of transactions done
                    sellStockVector.push_back(stockInfo);
                    std::thread sellThread(sellStock, sellStockVector[numOfSell]);
                    sellThread.detach();
                    numOfTransactions++;
                    numOfSell++;
                }

                isBuy = true;   // Run the buy statement next

            }
        }
    } catch (std::exception& ex)
        {
            std::cout << numOfTransactions << std::endl;
            std::cout << activeTreads << std::endl;
            std::cout << ex.what() << std::endl;
            return 0;
        }

    // Wait for server to print last information
    while (!serverDone){
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "End of program" << std::endl;

    return 0;
}