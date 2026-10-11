// Demo: builds a small catalog, sets a budget with per-category quotas,
// and runs a batch of purchase requests through the acquisition manager.

#include <iostream>

#include "bookmgmt/bookmgmt.h"

using namespace bookmgmt;

int main() {
    Catalog catalog;

    catalog.emplace<Book>("B001", "Clean Code", std::vector<std::string>{"Robert C. Martin"},
                          "978-0132350884", "Prentice Hall", 2008, Money::of(450));
    catalog.emplace<Book>("B002", "The C++ Programming Language",
                          std::vector<std::string>{"Bjarne Stroustrup"}, "978-0321563842",
                          "Addison-Wesley", 2013, Money::of(1200), 4, Binding::Hardcover);
    catalog.emplace<ElectronicResource>("R001", "IEEE Xplore Digital Library", "IEEE", 2026,
                                        Money::of(150), "https://ieeexplore.example",
                                        LicenseModel::AnnualSubscription, Money::of(2000));
    catalog.emplace<ElectronicResource>("R002", "MATLAB Campus Licence", "MathWorks", 2026,
                                        Money::of(400), "https://licensing.example/matlab",
                                        LicenseModel::Perpetual);

    std::cout << "=== Catalog ===\n";
    for (const Resource* r : catalog.all()) std::cout << r->summary() << "\n";

    std::cout << "\n=== Details of R001 ===\n" << catalog.get("R001");

    Budget budget(Money::of(20000));
    budget.setQuota(ResourceCategory::Book, {10, Money::of(8000)});
    budget.setQuota(ResourceCategory::ElectronicResource, {40, Money::of(12000)});

    AcquisitionManager acq(catalog, budget);

    std::cout << "\n=== Quotes ===\n";
    std::cout << "5 copies of B002  = " << acq.quote("B002", 5) << "\n";
    std::cout << "20 seats of R001  = " << acq.quote("R001", 20) << "  (incl. platform fee)\n";

    acq.processBatch({
        {"B001", 4},   // 1800  ok
        {"B002", 5},   // 7200 rejected: hardcover surcharge exceeds remaining book spend quota
        {"B001", 1},   // 450 approved: the preceding hardcover request was rejected
        {"R001", 20},  // 5000  ok
        {"R002", 25},  // 10000 rejected: e-resource unit quota (20 seats left)
        {"R002", 15},  // 6000  ok  -> e-resource spend 11000
        {"R002", 5},   // 2000  rejected: e-resource spend quota (1000 left)
        {"X999", 1},   // rejected: unknown id
    });

    std::cout << "\n=== Acquisition report ===\n";
    acq.printReport(std::cout);

    std::cout << "\n=== Budget ===\n";
    budget.print(std::cout);

    std::cout << "\n=== Holdings ===\n";
    for (const Resource* r : catalog.all())
        std::cout << "  " << r->id() << ": " << catalog.holdings(r->id())
                  << (r->isDigital() ? " seats" : " copies") << "\n";

    // Direct purchase: errors are reported with exceptions
    std::cout << "\n=== Direct purchase that breaks a quota ===\n";
    try {
        acq.purchase("B002", 10);
    } catch (const QuotaExceededError& e) {
        std::cout << "QuotaExceededError: " << e.what() << "\n";
    }
    // Part A uses a separate budget so the original walkthrough stays intact.
    std::cout << "\n=== Part A: new resource types ===\n";
    Catalog extended;
    extended.emplace<Journal>("J001", "Computing Journal", "1234-5678", 12,
                              "Publisher", 2026, Money::of(100), 3);
    extended.emplace<EBook>("E001", "Digital C++", std::vector<std::string>{"Author"},
                            "978-example", "Publisher", 2026, Money::of(20), "ebook-url",
                            FileFormat::EPUB, true, LicenseModel::Perpetual, Money::of(50));
    extended.emplace<ElectronicResource>("DB001", "Database", "Publisher", 2026,
                                         Money::of(10), "database-url");
    extended.emplace<AudioBook>("A001", "Audio C++", "Publisher", 2026, Money::of(15),
                                "audio-url", "Narrator", 180);
    extended.emplace<Thesis>("T001", "Library Systems", "IIITD", "PhD", "Supervisor", 2026);
    for (const Resource* r : extended.all()) std::cout << *r << "\n";
    Budget partABudget(Money::of(2000));
    partABudget.setQuota(ResourceCategory::Journal, {2, Money::of(600)});
    partABudget.setQuota(ResourceCategory::EBook, {3, Money::of(110)});
    partABudget.setQuota(ResourceCategory::ElectronicResource, {1, Money::of(10)});
    partABudget.setQuota(ResourceCategory::AudioBook, {2, Money::of(30)});
    partABudget.setQuota(ResourceCategory::Thesis, {1, Money{}});
    AcquisitionManager partA(extended, partABudget);
    std::cout << "Journal: 2 copies for 3 years = " << partA.quote("J001", 2) << "\n"
              << "EBook: 3 seats including platform fee = " << partA.quote("E001", 3) << "\n";
    partA.processBatch({{"J001", 2}, {"J001", 1}, {"E001", 3}, {"E001", 1},
                        {"DB001", 1}, {"A001", 2}, {"T001", 1}});
    std::cout << "T001 is intentionally free: one copy consumes a unit, but no money.\n";
    partA.printReport(std::cout);
    partABudget.print(std::cout);
    for (const Resource* r : extended.all())
        std::cout << r->id() << ": " << extended.holdings(r->id())
                  << (r->isDigital() ? " seats" : " copies") << "\n";
    std::cout << "\n=== Part B: hardcover and bulk pricing (pre-tax) ===\n";
    std::cout << "Hardcover B002, listed per-copy price: " << catalog.get("B002").unitPrice()
              << ", one copy: " << acq.quoteBeforeTax("B002", 1)
              << ", ten copies after discount: " << acq.quoteBeforeTax("B002", 10) << "\n";
    std::cout << "Paperback B001: 9 copies = " << acq.quoteBeforeTax("B001", 9)
              << ", 10 copies = " << acq.quoteBeforeTax("B001", 10) << "\n";
    std::cout << "Journal J001: 10 copies for 3 years = " << partA.quoteBeforeTax("J001", 10)
              << "\nElectronic R001: 50 seats = " << acq.quoteBeforeTax("R001", 50)
              << ", 60 seats = " << acq.quoteBeforeTax("R001", 60) << "\n";

    std::cout << "\n=== Part B: purchases across all categories ===\n";
    // A fresh catalogue and budget make these holdings independent of Part A.
    Catalog priced;
    priced.emplace<Book>("B001", "Hardcover C++", std::vector<std::string>{"Author"},
                         "isbn", "Publisher", 2026, Money::of(100), 1, Binding::Hardcover);
    priced.emplace<Journal>("J001", "Computing Journal", "1234-5678", 12,
                            "Publisher", 2026, Money::of(100), 2);
    priced.emplace<ElectronicResource>("R001", "Database", "Publisher", 2026,
                                       Money::of(10), "database-url",
                                       LicenseModel::Perpetual, Money::of(100));
    priced.emplace<EBook>("E001", "Digital C++", std::vector<std::string>{"Author"},
                          "isbn", "Publisher", 2026, Money::of(20), "ebook-url",
                          FileFormat::EPUB, true, LicenseModel::Perpetual, Money::of(50));
    priced.emplace<AudioBook>("A001", "Audio C++", "Publisher", 2026, Money::of(15),
                              "audio-url", "Narrator", 180,
                              LicenseModel::Perpetual, Money::of(30));
    priced.emplace<Thesis>("T001", "Paid thesis copies", "IIITD", "PhD", "Supervisor",
                           2026, Money::of(10));
    priced.emplace<Thesis>("T002", "Free deposited thesis", "IIITD", "PhD", "Supervisor", 2026);
    Budget taxedBudget(Money::of(10000));
    taxedBudget.setQuota(ResourceCategory::Book, {20, Money::of(1200)});
    taxedBudget.setQuota(ResourceCategory::Journal, {20, Money::of(2500)});
    taxedBudget.setQuota(ResourceCategory::ElectronicResource, {70, Money::of(1000)});
    taxedBudget.setQuota(ResourceCategory::EBook, {70, Money::of(1500)});
    taxedBudget.setQuota(ResourceCategory::AudioBook, {70, Money::of(1200)});
    taxedBudget.setQuota(ResourceCategory::Thesis, {20, Money::of(200)});
    AcquisitionManager taxed(priced, taxedBudget, {1000, 2000});
    std::cout << "Tax rates: print 10%, electronic 20%.\n"
              << "Bulk purchases: 10 print copies or 60 electronic seats.\n"
              << "T002 is intentionally free; T001 demonstrates paid thesis copies.\n";
    taxed.processBatch({{"B001", 10}, {"J001", 10}, {"R001", 60}, {"E001", 60},
                         {"A001", 60}, {"T001", 10}, {"T002", 1}});
    std::cout << "\nIntentional rejection: one more hardcover costs "
              << taxed.quoteBeforeTax("B001", 1) << " before tax and "
              << taxed.quote("B001", 1) << " after tax. Only "
              << *taxedBudget.spendRemaining(ResourceCategory::Book)
              << " remains in the book spending quota.\n";
    taxed.processBatch({{"B001", 1}});
    taxed.printReport(std::cout);
    taxedBudget.print(std::cout);
    std::cout << "\nPart B holdings (approved purchases only):\n";
    for (const Resource* r : priced.all())
        std::cout << r->id() << ": " << priced.holdings(r->id())
                  << (r->isDigital() ? " seats" : " copies") << "\n";
    std::cout << "\n=== Part C: title quotas and department budgets ===\n";
    Catalog departmentCatalog;
    departmentCatalog.emplace<Book>("B001", "Algorithms", std::vector<std::string>{"Author"},
                                     "isbn-1", "Publisher", 2026, Money::of(100));
    departmentCatalog.emplace<Book>("B002", "Physics", std::vector<std::string>{"Author"},
                                     "isbn-2", "Publisher", 2026, Money::of(100));
    departmentCatalog.emplace<Journal>("J001", "Computing Journal", "1234-5678", 12,
                                        "Publisher", 2026, Money::of(50), 2);
    departmentCatalog.emplace<ElectronicResource>("R001", "Research Database", "Publisher", 2026,
                                                 Money::of(10), "database-url",
                                                 LicenseModel::Perpetual, Money::of(20));
    departmentCatalog.emplace<EBook>("E001", "Digital Algorithms", std::vector<std::string>{"Author"},
                                     "isbn-3", "Publisher", 2026, Money::of(20), "ebook-url",
                                     FileFormat::EPUB, false, LicenseModel::Perpetual, Money::of(10));
    departmentCatalog.emplace<AudioBook>("A001", "Audio Algorithms", "Publisher", 2026,
                                         Money::of(15), "audio-url", "Narrator", 180,
                                         LicenseModel::Perpetual, Money::of(10));
    departmentCatalog.emplace<Thesis>("T001", "Printed Research", "IIITD", "PhD", "Supervisor",
                                     2026, Money::of(10));
    Budget defaultDepartment(Money::of(1000));
    Budget computerScience(Money::of(4000));
    Budget physics(Money::of(2000));
    // Both departments have their own one-title allowance.
    computerScience.setQuota(ResourceCategory::Book, {10, Money::of(1100), 1});
    physics.setQuota(ResourceCategory::Book, {2, Money::of(300), 1});
    for (Budget* department : {&computerScience, &physics}) {
        department->setQuota(ResourceCategory::Journal, {10, Money::of(1000), 2});
        department->setQuota(ResourceCategory::ElectronicResource, {20, Money::of(500), 2});
        department->setQuota(ResourceCategory::EBook, {20, Money::of(600), 2});
        department->setQuota(ResourceCategory::AudioBook, {20, Money::of(500), 2});
        department->setQuota(ResourceCategory::Thesis, {10, Money::of(200), 2});
    }
    AcquisitionManager departments(departmentCatalog, defaultDepartment, {1000, 2000});
    departments.addDepartment("Computer Science", computerScience);
    departments.addDepartment("Physics", physics);
    const int firstOrder = departments.purchase("B001", 5, "Computer Science").orderNo;
    const int secondOrder = departments.purchase("B001", 4, "Computer Science").orderNo;
    departments.processBatch({{"B002", 1, "Computer Science"}, {"B001", 2, "Physics"}});
    departments.processBatch({{"J001", 3, "Computer Science"}, {"R001", 6, "Computer Science"},
                              {"E001", 4, "Computer Science"}, {"A001", 5, "Computer Science"},
                              {"T001", 4, "Computer Science"}, {"J001", 2, "Physics"},
                              {"R001", 4, "Physics"}, {"E001", 3, "Physics"},
                              {"A001", 4, "Physics"}, {"T001", 3, "Physics"}});
    std::cout << "Repeated B001 purchases count as one title. CS rejects a second title,\n"
              << "while Physics can buy B001 using its independent budget and quotas.\n"
              << "\nComputer Science before cancellation:\n";
    computerScience.print(std::cout);
    std::cout << "\nPhysics budget:\n";
    physics.print(std::cout);

    std::cout << "\n=== Part C: cancellation and title-slot restoration ===\n";
    departments.cancel(firstOrder);
    std::cout << "After refunding the first order: CS spent " << computerScience.spent()
              << ", active titles " << computerScience.usageFor(ResourceCategory::Book).titles
              << ". The second B001 order still uses its title slot.\n";
    departments.cancel(secondOrder);
    std::cout << "After cancelling the last CS order for B001, another title is allowed: "
              << (departments.canPurchase("B002", 3, nullptr, "Computer Science") ? "yes" : "no")
              << "\n";
    departments.purchase("B002", 3, "Computer Science");
    departments.printReport(std::cout);
    std::cout << "\nComputer Science after refunds and replacement purchase:\n";
    computerScience.print(std::cout);
    std::cout << "Physics spending remains " << physics.spent() << " after CS refunds.\n";
    for (const Resource* r : departmentCatalog.all())
        std::cout << r->id() << ": " << departmentCatalog.holdings(r->id())
                  << (r->isDigital() ? " seats" : " copies") << "\n";

    std::cout << "\n=== Part C: next-year rollover ===\n";
    Budget nextYear = computerScience.rollover(Money::of(2000), 50);
    std::cout << "New allocation 2000.00 + 50% of CS unspent " << computerScience.remaining()
              << " = " << nextYear.total() << ". Quotas carry over; usage starts fresh.\n";
    std::cout << "Fresh-year spending: " << nextYear.spent()
              << "; book titles used: " << nextYear.usageFor(ResourceCategory::Book).titles << "\n";
    // Show the reset briefly, then give every category meaningful new-year usage.
    Catalog nextYearCatalog;
    nextYearCatalog.emplace<Book>("B001", "New Algorithms", std::vector<std::string>{"Author"},
                                  "isbn", "Publisher", 2027, Money::of(100));
    nextYearCatalog.emplace<Journal>("J001", "New Computing Journal", "1234-5678", 12,
                                     "Publisher", 2027, Money::of(50));
    nextYearCatalog.emplace<ElectronicResource>("R001", "New Database", "Publisher", 2027,
                                              Money::of(10), "database-url");
    nextYearCatalog.emplace<EBook>("E001", "New Digital Algorithms", std::vector<std::string>{"Author"},
                                  "isbn", "Publisher", 2027, Money::of(20), "ebook-url",
                                  FileFormat::PDF, false);
    nextYearCatalog.emplace<AudioBook>("A001", "New Audio Algorithms", "Publisher", 2027,
                                      Money::of(15), "audio-url", "Narrator", 180);
    nextYearCatalog.emplace<Thesis>("T001", "New Printed Research", "IIITD", "PhD", "Supervisor",
                                  2027, Money::of(10));
    AcquisitionManager nextYearPurchases(nextYearCatalog, nextYear, {1000, 2000});
    nextYearPurchases.processBatch({{"B001", 3}, {"J001", 2}, {"R001", 5},
                                    {"E001", 4}, {"A001", 3}, {"T001", 2}});
    std::cout << "\nNew-year purchases across all categories:\n";
    nextYearPurchases.printReport(std::cout);
    nextYear.print(std::cout);
    std::cout << "\n=== Part D: vendors and atomic acquisition ===\n";
    Catalog vendorCatalog;
    vendorCatalog.emplace<Book>("B001", "Vendor Hardcover", std::vector<std::string>{"Author"},
                                "isbn", "Publisher", 2027, Money::of(100), 1, Binding::Hardcover);
    vendorCatalog.emplace<Journal>("J001", "Vendor Journal", "1234-5678", 12,
                                   "Publisher", 2027, Money::of(50), 2);
    vendorCatalog.emplace<ElectronicResource>("R001", "Vendor Database", "Publisher", 2027,
                                             Money::of(10), "database-url",
                                             LicenseModel::Perpetual, Money::of(100));
    vendorCatalog.emplace<EBook>("E001", "Vendor EBook", std::vector<std::string>{"Author"},
                                 "isbn", "Publisher", 2027, Money::of(20), "ebook-url",
                                 FileFormat::EPUB, true, LicenseModel::Perpetual, Money::of(50));
    vendorCatalog.emplace<AudioBook>("A001", "Vendor AudioBook", "Publisher", 2027,
                                     Money::of(15), "audio-url", "Narrator", 180,
                                     LicenseModel::Perpetual, Money::of(30));
    vendorCatalog.emplace<Thesis>("T001", "Vendor Thesis", "IIITD", "PhD", "Supervisor",
                                 2027, Money::of(10));
    const std::vector<std::pair<std::string, int>> offerPrices = {
        {"B001", 80}, {"J001", 40}, {"R001", 8}, {"E001", 16}, {"A001", 12}, {"T001", 6}};
    for (const auto& [id, price] : offerPrices) {
        vendorCatalog.setVendorPrice(id, "Campus Supply", Money::of(price));
        vendorCatalog.setVendorPrice(id, "Library Market", Money::of(price + 5));
    }
    std::cout << "Vendor unit prices (pricing rules and taxes apply afterward):\n";
    for (const Resource* r : vendorCatalog.all()) {
        std::cout << r->id() << " listed " << r->unitPrice();
        for (const auto& offer : vendorCatalog.vendors(r->id()))
            std::cout << "; " << offer.vendor << " " << offer.unitPrice;
        std::cout << "\n";
    }
    Budget vendorBudget(Money::of(12000));
    vendorBudget.setQuota(ResourceCategory::Book, {30, Money::of(4000), 2});
    vendorBudget.setQuota(ResourceCategory::Journal, {30, Money::of(3000), 2});
    vendorBudget.setQuota(ResourceCategory::ElectronicResource, {200, Money::of(5000), 2});
    vendorBudget.setQuota(ResourceCategory::EBook, {200, Money::of(5000), 2});
    vendorBudget.setQuota(ResourceCategory::AudioBook, {200, Money::of(4000), 2});
    vendorBudget.setQuota(ResourceCategory::Thesis, {30, Money::of(500), 2});
    AcquisitionManager vendorAcq(vendorCatalog, vendorBudget, {1000, 2000});
    // Seed all six categories so a failed atomic batch visibly preserves real usage.
    vendorAcq.processBatch({{"B001", 1}, {"J001", 1}, {"R001", 1},
                            {"E001", 1}, {"A001", 1}, {"T001", 1}}, true);
    std::cout << "\nInitial successful atomic batch across all six categories:\n";
    vendorBudget.print(std::cout);

    std::cout << "\nAtomic rejection: journal requests of 20 and 15 copies together\n"
              << "exceed the 30-copy quota, which already has one copy in use.\n";
    const Money spendingBeforeBatch = vendorBudget.spent();
    const auto aborted = vendorAcq.processBatch({{"B001", 10}, {"J001", 20}, {"J001", 15}}, true);
    for (const auto& order : aborted)
        std::cout << order.resourceId << " x" << order.quantity << ": " << order.reason << "\n";
    std::cout << "Spending before failed batch: " << spendingBeforeBatch
              << "; afterward: " << vendorBudget.spent()
              << ". B001 holdings stay at " << vendorCatalog.holdings("B001") << ".\n";

    std::cout << "\nSuccessful atomic bulk purchase, selecting the cheapest vendors:\n";
    vendorAcq.processBatch({{"B001", 10}, {"J001", 10}, {"R001", 60},
                            {"E001", 60}, {"A001", 60}, {"T001", 10}}, true);
    vendorAcq.printReport(std::cout);
    vendorBudget.print(std::cout);
    std::cout << "\nPart D holdings:\n";
    for (const Resource* r : vendorCatalog.all())
        std::cout << r->id() << ": " << vendorCatalog.holdings(r->id())
                  << (r->isDigital() ? " seats" : " copies") << "\n";
    std::cout << "\n=== Part E: catalogue searches ===\n";
    std::cout << "Author search for 'author' (case-insensitive):\n";
    for (const Resource* r : vendorCatalog.searchAuthor("author")) std::cout << r->summary() << "\n";
    std::cout << "Exact ISBN search for 'isbn':\n";
    for (const Resource* r : vendorCatalog.searchIdentifier("isbn")) std::cout << r->summary() << "\n";
    std::cout << "Exact ISSN search for '1234-5678':\n";
    for (const Resource* r : vendorCatalog.searchIdentifier("1234-5678")) std::cout << r->summary() << "\n";
    std::cout << "Publication years 2026 through 2027 (inclusive):\n";
    for (const Resource* r : vendorCatalog.byYearRange(2026, 2027)) std::cout << r->summary() << "\n";

    std::cout << "\n=== Part E: borrowing and electronic sessions ===\n";
    for (const Resource* r : vendorCatalog.all()) {
        if (r->isDigital()) {
            vendorCatalog.openSession(r->id(), "P001");
            vendorCatalog.openSession(r->id(), "P002");
        } else {
            vendorCatalog.borrow(r->id(), "P001");
            vendorCatalog.borrow(r->id(), "P002");
        }
    }
    std::cout << "Two patrons use each resource; ownership and budgets stay unchanged.\n";
    for (const Resource* r : vendorCatalog.all())
        std::cout << r->id() << ": owned " << vendorCatalog.holdings(r->id())
                  << ", in use " << (r->isDigital() ? vendorCatalog.sessionUsers(r->id()).size()
                                                   : vendorCatalog.borrowers(r->id()).size())
                  << ", available " << vendorCatalog.available(r->id())
                  << (r->isDigital() ? " seats" : " copies") << "\n";
    try {
        vendorCatalog.openSession("R001", "P001");
    } catch (const LendingError& error) {
        std::cout << "Intentional duplicate-session rejection: " << error.what() << "\n";
    }
    // Return/close one patron's usage for every category, leaving another active.
    for (const Resource* r : vendorCatalog.all()) {
        if (r->isDigital()) vendorCatalog.closeSession(r->id(), "P001");
        else vendorCatalog.returnCopy(r->id(), "P001");
    }
    std::cout << "After P001 returns/closes, each resource still has one active patron:\n";
    for (const Resource* r : vendorCatalog.all())
        std::cout << r->id() << ": available " << vendorCatalog.available(r->id())
                  << (r->isDigital() ? " seats" : " copies") << "\n";

    std::cout << "\n=== Part F: currency-aware Money ===\n";
    const Money dollars = Money::of(125, 50, "USD");
    const Money moreDollars = Money::of(24, 50, "USD");
    const Money euros = Money::of(75, 0, "EUR");
    std::cout << "USD " << dollars << " + USD " << moreDollars
              << " = USD " << (dollars + moreDollars) << "\n"
              << "USD " << dollars << " - USD " << moreDollars
              << " = USD " << (dollars - moreDollars) << "\n";
    try { (void)(dollars + euros); }
    catch (const CurrencyMismatchError& error) {
        std::cout << "Intentional mixed-currency addition rejection: " << error.what() << "\n";
    }
    try { (void)(dollars - euros); }
    catch (const CurrencyMismatchError& error) {
        std::cout << "Intentional mixed-currency subtraction rejection: " << error.what() << "\n";
    }
    try { (void)(dollars < euros); }
    catch (const CurrencyMismatchError& error) {
        std::cout << "Intentional mixed-currency comparison rejection: " << error.what() << "\n";
    }

    std::cout << "\n=== Part F: USD acquisitions across all six categories ===\n";
    const auto usdPrice = [](int amount) { return Money::of(amount, 0, "USD"); };
    Catalog currencyCatalog;
    currencyCatalog.emplace<Book>("B001", "USD Hardcover", std::vector<std::string>{"Author"},
                                  "isbn", "Publisher", 2027, usdPrice(100), 1, Binding::Hardcover);
    currencyCatalog.emplace<Journal>("J001", "USD Journal", "1234-5678", 12,
                                     "Publisher", 2027, usdPrice(50), 2);
    currencyCatalog.emplace<ElectronicResource>("R001", "USD Database", "Publisher", 2027,
                                               usdPrice(10), "database-url",
                                               LicenseModel::Perpetual, usdPrice(100));
    currencyCatalog.emplace<EBook>("E001", "USD EBook", std::vector<std::string>{"Author"},
                                   "isbn", "Publisher", 2027, usdPrice(20), "ebook-url",
                                   FileFormat::EPUB, false, LicenseModel::Perpetual, usdPrice(50));
    currencyCatalog.emplace<AudioBook>("A001", "USD AudioBook", "Publisher", 2027,
                                       usdPrice(15), "audio-url", "Narrator", 180,
                                       LicenseModel::Perpetual, usdPrice(30));
    currencyCatalog.emplace<Thesis>("T001", "USD Printed Thesis", "IIITD", "PhD", "Supervisor",
                                   2027, usdPrice(10));
    currencyCatalog.setVendorPrice("B001", "US Campus Supply", usdPrice(80));
    currencyCatalog.setVendorPrice("B001", "US Library Market", usdPrice(90));
    Budget dollarBudget(usdPrice(10000));
    for (const Resource* r : currencyCatalog.all())
        dollarBudget.setQuota(r->category(), {100, usdPrice(5000), 2});
    AcquisitionManager dollarAcq(currencyCatalog, dollarBudget, {1000, 2000});
    dollarAcq.processBatch({{"B001", 10}, {"J001", 10}, {"R001", 60},
                            {"E001", 60}, {"A001", 60}, {"T001", 10}}, true);
    dollarAcq.printReport(std::cout);
    dollarBudget.print(std::cout);
    for (const Resource* r : currencyCatalog.all())
        std::cout << r->id() << ": " << currencyCatalog.holdings(r->id())
                  << (r->isDigital() ? " seats" : " copies") << "\n";
    return 0;
}
