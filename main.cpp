#include<iostream>
#include <sstream>
#include <memory>
#include<list>
#include<map>
#include <algorithm>
#include<numeric>

using namespace std;
std::ostringstream oss;

enum class OrderType{
    GoodTillCancel,
    FillAndKill
};

enum class Side{
    Buy, 
    sell
};

using Price = int32_t;
using Quantity = uint32_t;
using OrderId = uint32_t;

struct LevelInfo
{
    Price price_;
    Quantity quantity_ ;
};

using LevelInfos =  vector<LevelInfo> ;

class OrderbookLevelInfos
{
public:
    OrderbookLevelInfos(const LevelInfos& bids, const LevelInfos& asks)
        : bids_{ bids }
        , asks_{ asks }
    { 

    }

    const LevelInfos& GetBids() const { return bids_; }
    const LevelInfos& GetAsks() const { return asks_; }

private:
    LevelInfos bids_;
    LevelInfos asks_;
};

class Order{
    public :
            Order(OrderType orderType, Quantity quantity , Price price , OrderId orderId , Side side) : 
            orderType_ {orderType},
            initialQuantity_ {quantity},
            remainingQuantity_ {quantity},
            price_ {price},
            orderId_{orderId},
            side_{side}
            {

            }

            OrderId GetOrderId() const {return orderId_ ;}
            OrderType GetOrderType() const {return orderType_;}
            Price GetOrderPrice() const {return price_;}
            Side GetSide() const {return side_;}
            Quantity GetInitialQuantity() const  {return initialQuantity_;}
            Quantity GetRemainingQuantity() const {return remainingQuantity_;}
            Quantity FilledQuantity() const {return GetInitialQuantity() - GetRemainingQuantity();}
            bool IsFilled() const {return (GetRemainingQuantity() == 0 );}

            void Fill(Quantity quantity)
        {
            {
                if (quantity > GetRemainingQuantity())
                        std::ostringstream oss;
                        oss << "Order (" << GetOrderId() << ") cannot be filled for more than its remaining quantity.";
                        throw std::logic_error(oss.str());
                remainingQuantity_ -= quantity;
            }
        }
            

    private:
            Quantity initialQuantity_ ;
            Quantity remainingQuantity_ ;
            OrderType orderType_;
            Price price_;
            OrderId orderId_;
            Side side_;

};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

class OrderModify{
    public:
        OrderModify(OrderId orderId , Side side , Quantity quantity , Price price ):
        orderId_ {orderId},
        side_ {side},
        price_ {price},
        quantity_ {quantity}
        {

        }

        Quantity GetQuantity() const {return quantity_;}
        OrderId GetOrderId() const {return orderId_;}
        Side GetSide() const  { return side_;}
        Price GetPrice() const  {return price_;}

        OrderPointer toOrderPoint(OrderType type) const
        {
            return make_shared<Order>(type, GetQuantity(), GetPrice(), GetOrderId(), GetSide());
        }

    private:
        OrderId orderId_;
        Side side_;
        Price price_;
        Quantity quantity_;

};

struct TradeInfo{
    OrderId orderId_;
    Price price_;
    Quantity quantity_;

};

class Trade
{
    public:
        Trade(const TradeInfo& bidTrade , const  TradeInfo& askTrade) : 
        bidTrade_ {bidTrade},
        askTrade_ {askTrade}

        {

        }

        const TradeInfo& GetBitTrade() {return bidTrade_;}
        const TradeInfo& GetAskTrade() {return askTrade_;}


    private:
        TradeInfo bidTrade_;
        TradeInfo askTrade_;
    
    };

using Trades =  vector<Trade>;

class TradeBook{
    private:
        struct OrderEntry{
            OrderPointer order_{nullptr};
            OrderPointers::iterator location_;
        };

        map<Price ,OrderPointers , greater<Price>> bids_;
        map<Price , OrderPointers , less<Price>> asks_;

        unordered_map<OrderId , OrderEntry> orders_;

        bool canMatch(Side side , Price price) const 
        {
            if(side == Side::Buy){
                if(asks_.empty()){
                    return false;
                }

                const auto& [bestAsk , _] = *asks_.begin();
                return price >= bestAsk;
            }else{
                if(bids_.empty()){
                    return false;
                }

                const auto& [bestBid, _] = *bids_.begin();
                return price <= bestBid;
            }
        }

        Trades MatchOrders(){
            Trades trade;
            trade.reserve(orders_.size());

            while(true){
                if(bids_.empty() || asks_.empty()){
                    break;
                }
                auto& [bidPrice , bids] = *bids_.begin();
                auto& [askPrice , asks] = *asks_.begin();

                if(bidPrice < askPrice){
                    break;
                }
                while(bids.size() && asks.size()){
                    auto bid = bids.front();
                    auto ask = asks.front();

                    Quantity quantity = min(bid->GetRemainingQuantity() , ask->GetRemainingQuantity());

                    bid->Fill(quantity);
                    ask->Fill(quantity);

                    if(bid->IsFilled()){
                        bids.pop_front();
                        orders_.erase(bid->GetOrderId());
                    }
                    if(ask->IsFilled()){
                        asks.pop_front();
                        orders_.erase(ask->GetOrderId());
                    }
                    if(bids.empty()){
                        bids_.erase(bidPrice);
                    }
                    if(asks.empty()){
                        asks_.erase(askPrice);
                    }

                    trade.push_back(Trade
                        {
                            TradeInfo{ bid -> GetOrderId(), bid->GetOrderPrice() , quantity},
                            TradeInfo {ask->GetOrderId() , ask->GetOrderPrice() , quantity}
                        }
                    );
                }
            }
            if(!bids_.empty()){
                auto& [_ , bids] = *bids_.begin();
                auto& order = bids.front();

                if(order->GetOrderType() == OrderType::FillAndKill){
                    CancelOrder(order->GetOrderId());
                }

            }
            if(!asks_.empty()){
                auto& [_ , asks] = *asks_.begin();
                auto& order = asks.front();

                if(order->GetOrderType() == OrderType::FillAndKill){
                    CancelOrder(order->GetOrderId());
                }
            }

            return trade;
        };
    
    public:
    Trades AddOrder(OrderPointer order){
        if (orders_.find(order->GetOrderId()) != orders_.end()) {
        return  { };

            if(order->GetOrderType() == OrderType::FillAndKill && !canMatch(order->GetSide() , order->GetOrderPrice())){
                return { };
            }
        }
        OrderPointers::iterator  iterator;

        if(order->GetSide() == Side::Buy){
            auto& orders = bids_[order->GetOrderPrice()];
            orders.push_back(order);
            iterator = std::next(orders.begin() , orders.size() - 1);
        }
        else{
            auto& orders = asks_[order->GetOrderPrice()];
            orders.push_back(order);
            iterator = std::next(orders.begin() , orders.size() - 1);
        }

        orders_.insert({order->GetOrderId() , OrderEntry{order , iterator}});

        return MatchOrders();
    }
    void CancelOrder(OrderId orderId){
        if (orders_.find(orderId) == orders_.end()) {
            return ;
        }
        const auto& [order , orderIterator] = orders_.at(orderId);
        orders_.erase(orderId);

        if(order->GetSide() == Side::sell){
            auto price = order->GetOrderPrice();
            auto& orders = asks_.at(price);

            orders.erase(orderIterator);

            if(orders.empty()){
                asks_.erase(price);
            }
        }else{
            auto price = order->GetOrderPrice();
            auto& orders = bids_.at(price);

            orders.erase(orderIterator);
            if(orders.empty()){
                bids_.erase(price);
            }
        }

    }
    Trades MatchOrder(OrderModify order){
        if (orders_.find(order.GetOrderId()) == orders_.end()) {
            return { };
        }
        const auto& [existingorder , _] = orders_.at(order.GetOrderId());
        CancelOrder(order.GetOrderId());
        return AddOrder(order.toOrderPoint(existingorder->GetOrderType()));
    }
    size_t Size() const { return orders_.size();}

    OrderbookLevelInfos GetOrderInfos() { 
        LevelInfos bidInfos, askInfos;
        bidInfos.reserve(orders_.size());
        askInfos.reserve(orders_.size());

        auto CreateLevelInfos=  [] (Price price , const OrderPointers& orders){
            return LevelInfo{
                price, std::accumulate(orders.begin() , orders.end() , (Quantity)0,
                [] (Quantity runningSum , const OrderPointer& order)
                { return runningSum + order->GetRemainingQuantity();})
            };
        };
        for(const auto& [price , orders] : bids_){
            bidInfos.push_back(CreateLevelInfos(price , orders));
        }

        for(const auto& [price ,orders] :asks_){
            askInfos.push_back(CreateLevelInfos(price , orders));
        }
        return OrderbookLevelInfos{bidInfos , askInfos};
    };  
};


int main(){
    TradeBook orderBook;
    const OrderId orderId = 1;
    orderBook.AddOrder(make_shared<Order> (OrderType::GoodTillCancel ,10 ,10 , orderId ,Side::Buy )) ;
    cout << orderBook.Size() << endl;
    orderBook.AddOrder(make_shared<Order> (OrderType::GoodTillCancel ,100 ,110 , 1 ,Side::Buy )) ;
    // orderBook.CancelOrder(orderId);

    cout << orderBook.Size() << endl;

    return 0;
}