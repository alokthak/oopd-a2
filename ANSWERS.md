# Assignment 2 answers

## Completed: Parts A and B (Q1-Q6)

### Q1: Journal
Journal derives from Resource and represents print subscriptions. The unit price
is the annual price of one copy; costFor multiplies it by copies and subscription
years. Subscription years default to 1 and must be at least 1. Issues per year
must also be positive. Journals have their own category, quotas and report row.

### Q2: EBook
EBook derives from ElectronicResource and inherits its seat and platform-fee
pricing unchanged. FileFormat restricts formats to PDF, EPUB and HTML; invalid
enum values are rejected. Authors, ISBN, format and DRM status are printed after
the parent's details. EBook quotas are independent of ElectronicResource quotas.

Book and EBook duplicate author/ISBN storage, accessors and output. A shared
BibliographicInfo component could hold and format these fields without multiple
inheritance from Resource subclasses. This answer is also in EBook.h.

### Q3: AudioBook and Thesis
AudioBook models streamed audio, so ElectronicResource provides digital access,
seat licensing and platform-fee pricing. Duration is stored as positive whole
minutes. Thesis models a deposited print resource and derives directly from
Resource, avoiding unnecessary ISBN/binding or licensing fields. Its price
defaults to zero but paid copies are supported. Its university is also used as
publisher. Both choices are justified in the corresponding header comments.
Both types have separate categories and are included in budget reports.

### Validation
Each question has tests in tests/test_main.cpp and examples in examples/demo.cpp.
Original tests are retained. Pricing behaviour changes are those requested in
Part B. Parts C-G have not yet been attempted; macro-to-assert conversion is G.

### Q4: Hardcover surcharge
Book::costFor adds 20% for hardcover binding without modifying the listed unit
price. Paperback pricing is unchanged below the bulk threshold.

### Q5: Bulk discounts
Print items (Book, Journal and Thesis) receive 10% off the entire request when
buying at least 10 copies. Hardcover surcharge is applied before the discount;
both factors are combined before rounding. Journal pricing includes all
subscription years before discounting. ElectronicResource, EBook and AudioBook
charge full price for the first 50 seats and half price for additional seats;
the platform fee is never discounted. Thresholds apply per request, not to
accumulated holdings. Integer arithmetic rounds fractional minor units to the
nearest paise/cent, with ties rounded upward. The discounted electronic subtotal
is rounded once, rather than rounding each half-price seat independently.

### Q6: Taxes
TaxRates configures print and electronic rates in basis points (100 = 1%). Rates
must be non-negative and default to zero. Constructor configuration and
setTaxRates are supported. Tax is calculated on the discounted pre-tax amount,
including any platform fee, and rounded to the nearest minor unit, ties upward.
Resource::costFor and quoteBeforeTax provide pre-tax prices; quote returns the
post-tax price. canPurchase, purchase and processBatch all check spend quotas
and overall budget against this same post-tax amount. Unit quotas still count
copies/seats. PurchaseRecord stores preTaxCost and post-tax cost, so later tax
changes do not rewrite history. Reports show both amounts per order and both
approved-order totals; rejected orders are excluded from totals.
