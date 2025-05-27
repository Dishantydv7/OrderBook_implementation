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
    Sell // Fixed typo: 'sell' to 'Sell' to match enum case
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
    public:
            Order(OrderType orderType, Quantity quantity, Price price, OrderId orderId, Side side) : 
            orderType_{orderType},
            initialQuantity_{quantity},
            remainingQuantity_{quantity},
            price_{price},
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
                if (quantity > GetRemainingQuantity())
                {
                    std::ostringstream oss;
                    oss << "Order (" << GetOrderId() << ") cannot be filled for more than its remaining quantity.";
                    throw std::logic_error(oss.str());
                }
                remainingQuantity_ -= quantity;
            }

    private:
            OrderType orderType_;
            Quantity initialQuantity_;
            Quantity remainingQuantity_;
            Price price_;
            OrderId orderId_;
            Side side_;

};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

class OrderModify{
    public:
        OrderModify(OrderId orderId, Side side, Quantity quantity, Price price):
        orderId_{orderId},
        side_{side},
        quantity_{quantity},
        price_{price}
        {

        }

        Quantity GetQuantity() const {return quantity_;}
        OrderId GetOrderId() const {return orderId_;}
        Side GetSide() const  { return side_;}
        Price GetPrice() const  {return price_;}

        OrderPointer toOrderPoint(OrderType type) const
        {
            // Corrected parameter order: OrderType, Quantity, Price, OrderId, Side
            return make_shared<Order>(type, GetQuantity(), GetPrice(), GetOrderId(), GetSide());
        }

    private:
        OrderId orderId_;
        Side side_;
        Quantity quantity_;
        Price price_;

};

struct TradeInfo{
    OrderId orderId_;
    Price price_;
    Quantity quantity_;
};

class Trade
{
    public:
        Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade) : 
        bidTrade_{bidTrade},
        askTrade_{askTrade}
        {

        }

        const TradeInfo& GetBidTrade() const { return bidTrade_; }
        const TradeInfo& GetAskTrade() const { return askTrade_; }

    private:
        TradeInfo bidTrade_;
        TradeInfo askTrade_;
    
};

using Trades = vector<Trade>;

class OrderBook{
    private:
        struct OrderEntry{
            OrderPointer order_{nullptr};
            OrderPointers::iterator location_;
        };

        map<Price, OrderPointers, greater<Price>> bids_;
        map<Price, OrderPointers, less<Price>> asks_;
        unordered_map<OrderId, OrderEntry> orders_;

        bool CanMatch(Side side, Price price) const 
        {
            if(side == Side::Buy){
                if(asks_.empty())
                    return false;

                const auto& [bestAsk, _] = *asks_.begin();
                return price >= bestAsk;
            }
            else{
                if(bids_.empty())
                    return false;

                const auto& [bestBid, _] = *bids_.begin();
                return price <= bestBid;
            }
        }

        Trades MatchOrders()
        {
            Trades trades;
            trades.reserve(orders_.size());

            while(true)
            {
                if(bids_.empty() || asks_.empty())
                    break;

                auto& [bidPrice, bids] = *bids_.begin();
                auto& [askPrice, asks] = *asks_.begin();

                if(bidPrice < askPrice)
                    break;

                while(!bids.empty() && !asks.empty())
                {
                    auto bid = bids.front();
                    auto ask = asks.front();

                    Quantity quantity = min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());

                    bid->Fill(quantity);
                    ask->Fill(quantity);

                    trades.emplace_back(
                        TradeInfo{ bid->GetOrderId(), bid->GetOrderPrice(), quantity },
                        TradeInfo{ ask->GetOrderId(), ask->GetOrderPrice(), quantity }
                    );

                    if(bid->IsFilled())
                    {
                        bids.pop_front();
                        orders_.erase(bid->GetOrderId());
                    }

                    if(ask->IsFilled())
                    {
                        asks.pop_front();
                        orders_.erase(ask->GetOrderId());
                    }
                }

                if(bids.empty())
                    bids_.erase(bidPrice);

                if(asks.empty())
                    asks_.erase(askPrice);
            }

            if(!bids_.empty())
            {
                auto& [price, bids] = *bids_.begin();
                auto& order = bids.front();
                if(order->GetOrderType() == OrderType::FillAndKill)
                    CancelOrder(order->GetOrderId());
            }

            if(!asks_.empty())
            {
                auto& [price, asks] = *asks_.begin();
                auto& order = asks.front();
                if(order->GetOrderType() == OrderType::FillAndKill)
                    CancelOrder(order->GetOrderId());
            }

            return trades;
        }

    public:
        Trades AddOrder(OrderPointer order)
        {
            if (orders_.find(order->GetOrderId()) != orders_.end()) 
                return {};

            if(order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetOrderPrice()))
                return {};

            OrderPointers::iterator iterator;

            if(order->GetSide() == Side::Buy)
            {
                auto& orders = bids_[order->GetOrderPrice()];
                orders.push_back(order);
                iterator = prev(orders.end());
            }
            else
            {
                auto& orders = asks_[order->GetOrderPrice()];
                orders.push_back(order);
                iterator = prev(orders.end());
            }

            orders_.insert({order->GetOrderId(), OrderEntry{order, iterator}});
            return MatchOrders();
        }

        void CancelOrder(OrderId orderId)
        {
            if (orders_.find(orderId) == orders_.end()) 
                return;

            const auto& [order, iterator] = orders_.at(orderId);
            orders_.erase(orderId);

            if(order->GetSide() == Side::Sell)
            {
                auto price = order->GetOrderPrice();
                auto& orders = asks_.at(price);
                orders.erase(iterator);
                if(orders.empty())
                    asks_.erase(price);
            }
            else
            {
                auto price = order->GetOrderPrice();
                auto& orders = bids_.at(price);
                orders.erase(iterator);
                if(orders.empty())
                    bids_.erase(price);
            }
        }

        Trades ModifyOrder(OrderModify orderModify)
        {
            if (orders_.find(orderModify.GetOrderId()) == orders_.end()) 
                return {};

            const auto& [existingOrder, _] = orders_.at(orderModify.GetOrderId());
            CancelOrder(orderModify.GetOrderId());
            return AddOrder(orderModify.toOrderPoint(existingOrder->GetOrderType()));
        }

        size_t Size() const { return orders_.size(); }

        OrderbookLevelInfos GetOrderInfos() const 
        {
            LevelInfos bidInfos, askInfos;
            bidInfos.reserve(bids_.size());
            askInfos.reserve(asks_.size());

            auto CreateLevelInfo = [](Price price, const OrderPointers& orders)
            {
                return LevelInfo{ price, accumulate(orders.begin(), orders.end(), (Quantity)0, 
                    [](Quantity sum, const OrderPointer& order) { return sum + order->GetRemainingQuantity(); }) };
            };

            for(const auto& [price, orders] : bids_)
                bidInfos.push_back(CreateLevelInfo(price, orders));

            for(const auto& [price, orders] : asks_)
                askInfos.push_back(CreateLevelInfo(price, orders));

            return OrderbookLevelInfos(bidInfos, askInfos);
        }
};

int main()
{
    OrderBook orderBook;
    const OrderId orderId = 1;
    // Corrected parameter order: OrderType, Quantity, Price, OrderId, Side
    orderBook.AddOrder(make_shared<Order>(OrderType::GoodTillCancel, 100, 10, orderId, Side::Buy));
    cout << orderBook.Size() << endl; // Output: 1

    orderBook.CancelOrder(orderId);
    cout << orderBook.Size() << endl; // Output: 0

    return 0;
}