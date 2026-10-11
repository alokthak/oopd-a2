# Assignment 2 answers

## Completed: Parts A-D (Q1-Q12 and quota warnings)

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
Parts B-D. Parts E-G have not yet been attempted; macro-to-assert conversion is G.

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

### Q7: Different-title quotas
Quota has an optional maxTitles (unlimited by default). A title is identified by
its catalogue resource ID, not its display text, so two editions with different
IDs are distinct. Each budget tracks active units and spending by category and
ID, including purchases made before a title limit was configured. Repeated
purchases of an existing ID consume no additional title slot. Unit and spending
checks retain their existing priority; title limits are checked before the
overall budget. Legacy direct Budget calls without an ID remain valid without
a title limit, but an ID is mandatory if maxTitles is configured. Budget reports
show titles used/max alongside unit and spending usage.

### Q8: Cancellation
AcquisitionManager::cancel accepts an approved purchase order number and cancels
the entire order. It refunds the recorded post-tax cost, units and holdings,
without recalculating price, discount, platform fee or tax. The original record
stays unchanged and a separate cancellation record references its order number.
Cancellation amounts are positive refund amounts; totalBeforeTax and totalSpent
subtract them to report net totals. A title slot is restored only when all active
units for that ID in the charged budget have been refunded. Unknown, rejected,
already-cancelled and cancellation records cannot be cancelled. Missing or
insufficient holdings are rejected before state changes. Refund validation is
performed on a budget copy before applying changes. Invalid cancellations leave
budget, holdings and history unchanged.

### Q9: Department budgets
addDepartment registers an independent Budget by reference; it must outlive the
manager, as the original default Budget already must. Names are unique and
non-empty, and the same budget cannot be shared by multiple departments. Requests
and records carry a department name. An empty name retains the original default
budget behaviour. Unknown departments are rejected. Approval, purchase and batch
processing all use the selected department's quotas and overall budget. Refunds
return to the department stored in the original order. Holdings remain shared
in the catalogue; spending, unit usage and title usage are department-specific.

### Q10: Year-end rollover
Budget::rollover(nextYearAllocation, carryPercent) creates a new independent
budget with the fresh allocation plus 0..100% of this year's unspent money.
Carry is rounded to the nearest minor unit, ties upward. All category quota
limits are copied, but spending, unit usage and active-title usage reset to zero.
The old budget is unchanged and retains its orders/usage. Negative allocations
and percentages outside 0..100 are rejected. Overflow in the new total is checked.

### Unnumbered Part C requirement: warnings
The PDF includes a trailing line about a category passing 80% of its unit or
spending quota. Budget::warnings returns current warnings and Budget::print prints
them. Exactly 80% does not warn; strictly greater than 80% does. Categories with
no quota and zero spending limits do not generate spending warnings. Refunds can
clear warnings. Threshold comparisons use integer arithmetic without multiplying
large limits by four. The demo buys resources in every category for both departments. It shows the
rollover's fresh usage briefly, then purchases in all six categories before
printing the new-year budget report.

### Q11: All-or-nothing acquisition
processBatch(requests, true) enables atomic processing; the default false keeps
the existing independent approval behaviour. Atomic mode stages independent
copies of the affected department budgets, accumulated holdings and order
records. Each request is checked against usage from earlier requests, including
post-tax spending, unit quotas and distinct-title quotas. If any request fails,
no budget or holdings change: every request is recorded as rejected, with its
own failure or an explanation that another request aborted the batch. Existing
purchases/history are retained. Successful batches apply the prepared budgets
and holdings together. Empty batches have no effect. Each approved order can
subsequently be cancelled through the existing cancellation API.

### Q12: Vendors
Catalog::setVendorPrice(resourceId, vendor, unitPrice) adds or updates a named
offer; Catalog::vendors returns the recorded offers. Prices are per copy, seat,
or annual journal subscription, matching each resource's existing unitPrice
meaning. Offers belong to the catalogue entry and disappear when it is removed.
Vendor names must be non-empty and prices non-negative. Quotes, approval checks,
ordinary purchases and both batch modes select the lowest unit-price offer;
equal prices are resolved by alphabetical vendor name for deterministic results.
If there are no offers, the catalogue price remains the fallback.

The new virtual costAtUnitPrice applies each class's existing pricing rules to
a supplied price without mutating the resource. Hardcover surcharge, print bulk
discounts, journal subscription years and half-price electronic seats all still
apply; platform fees are resource-specific and unchanged between vendors. Taxes
are applied afterward. PurchaseRecord stores the selected vendor and the original
pre-/post-tax amounts. Cancellation records retain that vendor and refund the
original cost even if offers later change. Reports display the vendor for orders
with explicit offers; empty vendor fields identify catalogue-price fallback.
The demo includes offers for all six resource types, successful atomic batches,
and a rejected cumulative-quota batch that preserves non-zero existing usage.
