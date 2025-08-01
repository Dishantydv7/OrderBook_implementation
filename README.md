# Limit Order Book - C++ Implementation

This project implements a **limit order book** for a trading system using C++. It supports placing, modifying, and canceling orders while automatically matching opposing orders based on a price-time priority system. The implementation is designed to simulate a basic electronic trading platform.

## Data Structures

The core structure of the order book consists of two `std::map` containers:

* `bids_`: A map from price to a list of buy orders (`OrderPointers`), sorted in **descending** order using `std::greater<Price>`.
* `asks_`: A map from price to a list of sell orders, sorted in **ascending** order using `std::less<Price>`.

Each price level in the book holds a `std::list` of `std::shared_ptr<Order>` to maintain insertion order (time priority) and allow fast deletion.

A separate `unordered_map<OrderId, OrderEntry>` called `orders_` stores metadata for fast access, cancelation, or modification by order ID. Each `OrderEntry` stores a pointer to the actual `Order` and its position (`list::iterator`) in the corresponding list.

## Order Types

Two types of orders are supported:

* `GoodTillCancel` (GTC): Orders remain in the book until they are completely filled or manually canceled.
* `FillAndKill` (FAK): Orders must be matched immediately upon insertion. If no suitable match exists, they are discarded.

## Matching Engine Logic

The `AddOrder` method is the entry point for placing a new order. If an order is eligible to match, `MatchOrders` is called to match it against the best available counter-orders.

### Matching Steps

1. **Check if the order should be matched**:

   * For a buy order, check if there are sell orders with prices less than or equal to the buy price.
   * For a sell order, check if there are buy orders with prices greater than or equal to the sell price.

2. **Match Orders While Possible**:

   * Select the best bid and best ask.
   * Match orders at those price levels one by one, from the front of the list (earliest orders).
   * Determine the minimum of the remaining quantities for both sides.
   * Fill that quantity and update each order.
   * If an order is completely filled, remove it from its list and the global map.
   * If the list at a price level becomes empty, remove the price level entry.

3. **Handle FillAndKill Orders**:

   * After matching, if the best remaining order is `FillAndKill` and it hasn't been fully filled, it is canceled.

All matched trades are stored in a vector of `Trade` objects, where each `Trade` consists of the matched buyer and seller information (OrderId, price, and quantity).

## Modifying Orders

To modify an existing order, the `MatchOrder` method is used. It performs the following steps:

* Locate the existing order in the `orders_` map.
* Cancel the order using `CancelOrder`.
* Create a new order from the modified parameters and the existing order type.
* Insert the new order into the order book using `AddOrder`.

This ensures atomic replacement of the order without affecting the rest of the book.

## Canceling Orders

The `CancelOrder` method finds the order in `orders_` and removes it from both:

* The corresponding price level list (`bids_` or `asks_`)
* The `orders_` map

If the list at a particular price level becomes empty, the entry for that price is also removed from the map.

## Viewing Book Depth

The `GetOrderInfos` method returns the current state of the order book using the `OrderbookLevelInfos` class. It iterates through each price level in both `bids_` and `asks_` and computes the total remaining quantity at that price. The result is a vector of `LevelInfo` objects for both sides.

This is useful for inspecting market depth or building a UI that displays the top-of-book or full book view.

## Sample Usage (from `main` function)

```cpp
TradeBook orderBook;
const OrderId orderId = 1;
orderBook.AddOrder(make_shared<Order>(OrderType::GoodTillCancel, 10, 10, orderId, Side::Buy));
cout << orderBook.Size() << endl;
orderBook.AddOrder(make_shared<Order>(OrderType::GoodTillCancel, 100, 110, 1, Side::Buy));
cout << orderBook.Size() << endl;
```

This sample adds two buy orders to the order book and prints the number of active orders.

## Technical Highlights

* Uses `std::shared_ptr` for safe dynamic memory management of orders.
* Bid and ask maps allow O(log N) insertion and deletion by price level.
* Uses `std::list` for O(1) removal of specific orders using stored iterators.
* Custom comparator for price ordering (`greater` for bids, `less` for asks\`).
* Exception handling is included for overfill conditions in `Order::Fill()`.
* Modular classes like `Order`, `OrderModify`, `Trade`, and `OrderbookLevelInfos` make the codebase extendable and easy to test.

This implementation is designed to be minimal yet faithful to how an actual trading system operates. It can serve as a base for simulating order flow, backtesting trading strategies, or extending toward a more complex exchange architecture.


  ## Limitations

- Single-threaded implementation
- Basic error handling
- Simplified price model

