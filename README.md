# Order Book Implementation

A C++ limit order book implementation for financial trading systems. Handles order matching, cancellation, and price-time priority execution.

## Features

- Supports order types:
  - Good-Till-Cancel (GTC)
  - Fill-and-Kill (FAK)
- Implements basic order matching engine
- Maintains bid/ask levels
- Handles order modifications
- Provides order book depth information

## Order Types

- **Good-Till-Cancel (GTC)**: 
  - Remains active until explicitly canceled
  - Persists through partial fills

- **Fill-and-Kill (FAK)**:
  - Either fills immediately (fully/partially) 
  - Or gets canceled entirely

  ## Limitations

- Single-threaded implementation
- Basic error handling
- Simplified price model

