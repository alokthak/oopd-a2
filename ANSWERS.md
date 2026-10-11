# Assignment 2 answers

## Completed: Part A (Q1-Q3)

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
Original public function behaviour and original test cases are preserved.
Parts B-G have not yet been attempted; the macro-to-assert change belongs to G.
