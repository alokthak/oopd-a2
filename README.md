# BookManagement — a C++17 library for library acquisitions

A small, dependency-free C++ library for cataloguing library resources and
buying them against a budget with per-category purchase quotas. It is written
as a starting point for an assignment: the core is complete and tested, and
the places intended for extension are marked `EXTENSION POINT` in the source.

## Building

```sh
cmake -S . -B build
cmake --build build
./build/bookmgmt_tests     # runs the tests
./build/demo               # walkthrough of every feature
```

Without CMake:

```sh
g++ -std=c++17 -Wall -Wextra -Iinclude src/*.cpp examples/demo.cpp -o demo
g++ -std=c++17 -Wall -Wextra -Iinclude src/*.cpp tests/test_main.cpp -o tests
```

## Layout

```
include/bookmgmt/
  bookmgmt.h            includes everything below
  Money.h               currency amounts (stored as whole paise/cents)
  Resource.h            abstract base class + ResourceCategory
  Book.h                print book
  ElectronicResource.h  online resource licensed per user seat
  Budget.h              overall budget + per-category Quota
  Catalog.h             owns resources, tracks holdings, searching
  Acquisition.h         purchase requests, orders, history, reports
  Exceptions.h          exception classes
src/                    implementations
examples/demo.cpp       end-to-end example
tests/test_main.cpp     tests (standard assert, no external framework)
```

## Design

### Class hierarchy

```
Resource (abstract)
├── Book
└── ElectronicResource
```

Every `Resource` has an id, title, publisher, year and a unit price, and
provides two virtual functions that subclasses override: `category()` and
`costFor(quantity)`. "Quantity" means copies for print items and
concurrent-user seats for electronic ones.

| Class | `unitPrice` means | `costFor(n)` |
|---|---|---|
| `Book` | price per copy | `unitPrice × n` |
| `ElectronicResource` | price per seat | `platformFee + unitPrice × n` |

`Resource::print()` writes the fields every resource has, then calls the
virtual function `printDetails()`, which each subclass overrides to print its
own fields.

### Money

`Money` stores a whole number of paise/cents (`int64_t`), so adding up costs
never suffers floating-point rounding errors. Create values with
`Money::of(125, 50)` (125.50) or `Money::fromMinor(12550)`.

### Quotas and budget

`Budget` holds a total spending limit plus an optional `Quota{maxUnits, maxSpend}`
per `ResourceCategory`. A purchase is allowed only if it fits the category's
unit quota, the category's spend quota, and the overall remaining budget, in
that order. Categories without a quota are limited only by the overall budget.

- `check(...)` returns the reason a purchase would fail (empty string = OK).
- `commit(...)` records it, or throws `QuotaExceededError` /
  `BudgetExceededError` and leaves the budget unchanged.

### Acquisition

`AcquisitionManager` connects a `Catalog` and a `Budget`:

- `quote(id, n)` — price without buying.
- `canPurchase(id, n, &reason)` — checks without buying.
- `purchase(id, n)` — buys or throws; updates budget and holdings.
- `processBatch(requests)` — processes each request in order, never throws
  for a rejected request, and records every outcome (approved or not) in
  `history()`.
- `printReport(os)` — order history and total spend.

### Ownership

`Catalog` owns resources through `std::unique_ptr<Resource>`; copying a
`Resource` is disabled so a derived object can't be accidentally sliced into
a base one. Searches return `const Resource*` pointers that stay valid until
the item is removed.

## Minimal usage

```cpp
#include "bookmgmt/bookmgmt.h"
using namespace bookmgmt;

Catalog catalog;
catalog.emplace<Book>("B001", "Clean Code", std::vector<std::string>{"Robert C. Martin"},
                      "978-0132350884", "Prentice Hall", 2008, Money::of(450));

Budget budget(Money::of(10000));
budget.setQuota(ResourceCategory::Book, {5, Money::of(2000)});

AcquisitionManager acq(catalog, budget);
acq.purchase("B001", 3);            // OK: 1350.00
acq.canPurchase("B001", 3);         // false: unit quota (2 left)
```

## Extension exercises

Difficulty: ★ easy, ★★ moderate, ★★★ challenging. For every exercise, add
tests to `tests/test_main.cpp` and extend `examples/demo.cpp` to show the new
feature. Exercises 1 and 2 should be done first; later exercises may use
the classes they add.

**Extending the class hierarchy**

1. ★★ **Journal.** Add a class `Journal`, derived from `Resource`, for a print
   journal bought as a subscription.
   - Extra fields: ISSN, issues per year, and subscription length in years
     (default 1; must be at least 1).
   - `unitPrice` is the annual subscription price for one copy.
   - `costFor(copies)` = `unitPrice × copies × subscriptionYears`.
   - Add `Journal` to `ResourceCategory`, `categoryName()` and
     `kAllCategories`, so budgets can set a journal quota and reports list it.
   - Override `printDetails()` to print the new fields.

2. ★★ **EBook.** Add a class `EBook`, derived from `ElectronicResource`.
   - Extra fields: authors, ISBN, file format (PDF, EPUB or HTML) and whether
     it is DRM-protected.
   - Pricing is inherited unchanged from `ElectronicResource`.
   - It needs its own `ResourceCategory` value so e-books can have a quota
     separate from other electronic resources.
   - `printDetails()` should print the inherited fields (call the parent's
     version) followed by the new ones.
   - Question to answer in comments: an e-book is also a kind of book. What
     code is now duplicated between `Book` and `EBook`, and how could you
     avoid it?

3. ★ Add an `AudioBook` (narrator, duration in minutes) and a `Thesis`
   (university, degree, supervisor; usually free of cost).

4. ★★ Add a `Magazine` derived from `Journal` whose cost also includes
   postage for each issue.

**Pricing**

5. ★ Hardcover books cost 20% more than their listed unit price.
6. ★★ Bulk discount: 10% off when buying 10 or more copies of a print item.
   For electronic resources, seats beyond the 50th cost half price.
7. ★★ Taxes: print and electronic items are taxed at different rates.
   Reports should show both pre-tax and post-tax totals.

**Quotas and budget**

8. ★ Add a limit on the number of *different titles* bought per category.
9. ★★ Support cancelling an approved order: refund the budget and reduce the
   holdings. Keep the original record and add a cancellation record.
10. ★★ Department budgets: each department (Computer Science, Physics, ...)
    has its own budget and quotas; a purchase request names the department.
11. ★★ Year-end rollover: carry a configurable percentage of unspent budget
    into next year's budget.
12. ★★ Warnings: print a warning when a category passes 80% of its quota.

**Acquisition**

13. ★ Add an option to make `processBatch` all-or-nothing: if any request
    would be rejected, buy nothing.
14. ★★ Requests carry a priority; process higher-priority requests first.
15. ★★★ Vendors: the same title is sold by several vendors at different
    prices; buy from the cheapest vendor.

**Catalog**

16. ★ Search by author, ISBN/ISSN, and by a range of years.
17. ★★ Save the catalog to a text file and load it back, creating the
    correct derived class for each line.
18. ★★ Lending: patrons borrow and return print copies (limited by the number
    of copies held), and use electronic seats (limited by seats licensed).

**Money**

19. ★★ Add a currency code to `Money`; adding amounts in different
    currencies should throw an exception.

## Student details

- Name: Alok Kumar Thakur
- Roll No.: MT26018

New code files begin with the roll number and name. Modified instructor-supplied
files retain their original opening comments.

## Generative AI disclosure

OpenAI Codex was used to help implement Parts A-G, write and revise tests and demo
examples, explain the supplied C++ code and build process, and prepare the written
answers. Its changes were reviewed by the student, who handled commits and pushes.
The implementation was checked with CMake/CTest and warning-enabled compilation.
AI-assisted source changes remain subject to the student's review and responsibility.

### Implementation prompts

The following implementation and revision prompts were provided in this chat.
The assignment PDF, original source files, terminal output and screenshots were
also supplied as context where applicable.

1. "Let's the follow the convention of committing all the changes after completing each part i.e. A, B, C, ..., G."
2. "I have made the initial commit and pushed to the remote repo. Now, make all the changes for completing Part A. Make sure to follow the general instructions stated in the pdf."
3. "Don't commit. Just make the changes so that I can manually go through all of your code changes. I will commit and push those changes myself."
4. "I have pushed Part A changes. Now implement Part B."
5. "Please check this output. I see a lot of zeros in output for Part B. Also, some zeros in Part A."
6. "Don't you think that it is better to have examples with non-zero outputs ?"
7. "So, according to you, we should not make any changes further, right ?"
8. "So, what should we do ?"
9. "I noticed now, that the original code i.e. before Part A is also having many zeros in the output. Was that the case ?"
10. "You changed the format of the ids of the resources in the holdings from something like B001 to only alphabets."
11. "So, please do so." (Restore the established resource ID format.)
12. "I have now committed and pushed Part B changes. Now, implement code changes for Part C."
13. "Part C's output also contain a lot of zeros."
14. "Please do not make examples which results in mostly 0 outputs. Those kind of outputs don't look good."
15. "I have committed and pushed the changes for Part C. Now, implement Part D."
16. "I have committed and pushed changes for Part D. Now, implement Part E."
17. "Why are you saving demo's catalogue to `build/part_e_catalog.txt` ?"
18. "Please check the Assignment_2.pdf file. It got changed and we were not informed about it. I just noticed that and replaced it with the new version of that pdf. Please check and tell what all have to be changed again."
19. "So, make the code changes in Part C and Part E to align with the revised PDF."
20. "I have pushed the changes. Now implement Part F."
21. "I have pushed Part F code changes. Now implement Part G."
22. "My Roll No. is MT26018 and my name is Alok Kumar Thakur." The accompanying instructions were: "Each code file (apart from the supplied ones) must have the RollNo. & Name as a comment in the very beginning." and "As discussed, generative AI is allowed, but the prompts should be disclosed in the README file."

The revised PDF removed quota warnings and catalogue save/load. Those features
were removed from the final implementation; the persistence-related prompts
above describe that earlier stage of the work.

### Explanation and workflow prompts

The following additional prompts requested explanations or workflow guidance;
quoted commands, source snippets and pasted output accompanied some requests.

- "This folder contains a pdf explaining the assignment and the extracted zip folder containing all the files of the original assignment. Please go through this folder and tell me whether I can do this on my Macbook Pro M4 Pro or will I have to do it on an already setup artix base Linux Virtual Machine ?"
- "How to use CMake ?"
- "What does the above command do ?" (`-DCMAKE_BUILD_TYPE=Debug`)
- "Does the above command create threads indefinitely i.e. is there no limit to the number of threads being created ?" (`cmake --build build --parallel`)
- "Can you explain this output by referring to the lines in the CMakeLists.txt file ?" (Pasted build output.)
- "Explain the above output." (Pasted demo output.)
- "Explain how catalog.all() function returns the vector<Resource> containing all 4 resources ?" (With the loop printing summaries.)
- "But how does where() function know that it has to pass the argument of Resource& by iterating through the vector<Resource*> object which called this all() function ?"
- "What is the use of stating the quote ? And why are those specific numbers and categories shown ?"
- "What does std::left and std::right do ?"
- "Explain the above code, w.r.t the below output:" (The order-report formatting code and its output.)
- "Ok, Now I will proceed with the assignment. Have you read the submission instructions ?"
- "I am creating a github repository and thinking of pushing the original assignment without doing any modifications to that github repo. After that I will start solving the assignment. Sounds good ?"
- "Why are you putting /demo and /bookmgmt_tests in .gitignore file ? They were part of the original repo, right ?"
- "How to check what is the remote branch and the repo from my local git repo ?"
- "I want to change the remote repo. How to do it ?"
- "How to set the upstream branch ?"
- "What will this command do ?" (`git push -u origin main`)
- "My repository is named oopd-a2."
- "What does drm mean ?"
- "What commands should I run to test the changes using the cmake file after every time you make a change ?"
- "Is the above command not required after each time you make changes in the code ?" (`cmake -S . -B build`, followed by the proposed configure/build/test/demo sequence.)
- "Please explain why running the above command not necessary everytime ?"
- "Should I commit and push now ?"
- "Was adding Pricing.h and not Pricing.cpp intentional ?"
- "Why did you use inline with it ?"
- "Give the commit message"
