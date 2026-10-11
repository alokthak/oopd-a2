// Minimal self-contained test runner (no external framework needed).

#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#include "bookmgmt/bookmgmt.h"

using namespace bookmgmt;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond \
                      << "\n";                                                   \
        }                                                                        \
    } while (0)

#define CHECK_THROWS(expr, ExType)            \
    do {                                      \
        bool thrown_ = false;                 \
        try {                                 \
            (void)(expr);                     \
        } catch (const ExType&) {             \
            thrown_ = true;                   \
        } catch (...) {                       \
        }                                     \
        CHECK(thrown_ && "expected " #ExType); \
    } while (0)

static void testMoney() {
    CHECK(Money::of(12, 5).toString() == "12.05");
    CHECK(Money::of(-3, 50).toString() == "-3.50");
    CHECK(Money::fromMinor(7).toString() == "0.07");
    CHECK(Money::of(10) + Money::of(0, 50) == Money::fromMinor(1050));
    CHECK(Money::of(3) * 4 == Money::of(12));
    CHECK(Money::of(1) < Money::of(2));
    CHECK_THROWS(Money::of(1, 100), std::invalid_argument);
}

static void testResourcesAndCost() {
    Book b("B1", "T", {"A", "B", "C"}, "isbn", "P", 2020, Money::of(100));
    CHECK(b.category() == ResourceCategory::Book);
    CHECK(!b.isDigital());
    CHECK(b.costFor(3) == Money::of(300));
    CHECK_THROWS(b.costFor(0), std::invalid_argument);
    CHECK(joinAuthors(b.authors()) == "A, B and C");

    ElectronicResource e("R1", "DB", "P", 2026, Money::of(10), "url",
                         LicenseModel::AnnualSubscription, Money::of(100));
    CHECK(e.isDigital());
    CHECK(e.costFor(5) == Money::of(150));

    CHECK(e.category() == ResourceCategory::ElectronicResource);

    // Polymorphism through a base-class reference
    const Resource& r = e;
    CHECK(r.costFor(1) == Money::of(110));
    std::ostringstream os;
    os << r;
    CHECK(os.str().find("platform fee: 100.00") != std::string::npos);

    CHECK_THROWS(Book("", "T", {}, "", "", 2000, Money::of(1)), std::invalid_argument);
    CHECK_THROWS(Book("B", "T", {}, "", "", 2000, Money::fromMinor(-1)),
                 std::invalid_argument);
}

static void testCatalog() {
    Catalog c;
    c.emplace<Book>("B1", "Clean Code", std::vector<std::string>{"M"}, "i", "P", 2008,
                    Money::of(1));
    c.emplace<Book>("B2", "Clean Architecture", std::vector<std::string>{"M"}, "i", "P",
                    2017, Money::of(1));
    c.emplace<ElectronicResource>("R1", "ACM Digital Library", "ACM", 2026, Money::of(1),
                                  "url");

    CHECK(c.size() == 3);
    CHECK(c.contains("B1"));
    CHECK(c.find("nope") == nullptr);
    CHECK_THROWS(c.get("nope"), NotFoundError);
    CHECK_THROWS(c.emplace<Book>("B1", "dup", std::vector<std::string>{}, "", "", 1,
                                 Money::of(1)),
                 DuplicateIdError);

    CHECK(c.searchTitle("clean").size() == 2);
    CHECK(c.byCategory(ResourceCategory::ElectronicResource).size() == 1);
    CHECK(c.where([](const Resource& r) { return r.isDigital(); }).size() == 1);

    CHECK(c.holdings("B1") == 0);
    c.addHoldings("B1", 3);
    CHECK(c.holdings("B1") == 3);
    CHECK_THROWS(c.addHoldings("B1", -5), std::invalid_argument);

    c.remove("R1");
    CHECK(c.size() == 2);
    CHECK_THROWS(c.remove("R1"), NotFoundError);
}

static void testBudget() {
    Budget b(Money::of(1000));
    b.setQuota(ResourceCategory::Book, {5, Money::of(400)});

    CHECK(b.check(ResourceCategory::Book, 2, Money::of(200)).empty());
    CHECK(!b.check(ResourceCategory::Book, 6, Money::of(10)).empty());   // units
    CHECK(!b.check(ResourceCategory::Book, 1, Money::of(401)).empty());  // spend
    CHECK(!b.check(ResourceCategory::ElectronicResource, 1, Money::of(1001)).empty());  // overall
    CHECK(b.check(ResourceCategory::ElectronicResource, 1, Money::of(900)).empty());    // no quota

    b.commit(ResourceCategory::Book, 4, Money::of(300));
    CHECK(b.spent() == Money::of(300));
    CHECK(*b.unitsRemaining(ResourceCategory::Book) == 1);
    CHECK(*b.spendRemaining(ResourceCategory::Book) == Money::of(100));
    CHECK(!b.unitsRemaining(ResourceCategory::ElectronicResource).has_value());

    CHECK_THROWS(b.commit(ResourceCategory::Book, 2, Money::of(10)), QuotaExceededError);
    CHECK_THROWS(b.commit(ResourceCategory::ElectronicResource, 1, Money::of(800)),
                 BudgetExceededError);
    CHECK_THROWS(b.commit(ResourceCategory::ElectronicResource, 0, Money::of(1)),
                 std::invalid_argument);
    CHECK(b.spent() == Money::of(300));  // failed commits changed nothing
}

static void testAcquisition() {
    Catalog c;
    c.emplace<Book>("B1", "Book", std::vector<std::string>{"A"}, "i", "P", 2020,
                    Money::of(100));
    c.emplace<ElectronicResource>("R1", "DB", "P", 2026, Money::of(10), "url",
                                  LicenseModel::AnnualSubscription, Money::of(50));
    Budget b(Money::of(500));
    b.setQuota(ResourceCategory::Book, {3, Money::of(1000)});
    AcquisitionManager acq(c, b);

    CHECK(acq.quote("R1", 5) == Money::of(100));
    std::string why;
    CHECK(acq.canPurchase("B1", 3, &why) && why.empty());
    CHECK(!acq.canPurchase("B1", 4, &why) && !why.empty());
    CHECK(!acq.canPurchase("nope", 1, &why));

    const auto& rec = acq.purchase("B1", 2);
    CHECK(rec.approved && rec.cost == Money::of(200) && rec.orderNo == 1);
    CHECK(c.holdings("B1") == 2);

    CHECK_THROWS(acq.purchase("B1", 2), QuotaExceededError);
    CHECK_THROWS(acq.purchase("nope", 1), NotFoundError);
    CHECK(acq.history().size() == 1);  // exceptions don't record

    auto res = acq.processBatch({{"R1", 10}, {"R1", 100}, {"B1", 1}, {"zzz", 1}, {"B1", 0}});
    CHECK(res.size() == 5);
    CHECK(res[0].approved && res[0].cost == Money::of(150));
    CHECK(!res[1].approved);  // 1050 > remaining 150
    CHECK(res[2].approved);
    CHECK(!res[3].approved && res[3].reason.find("not found") != std::string::npos);
    CHECK(!res[4].approved);
    CHECK(acq.totalSpent() == Money::of(450));
    CHECK(b.spent() == acq.totalSpent());
    CHECK(c.holdings("R1") == 10 && c.holdings("B1") == 3);
    CHECK(acq.history().size() == 6);
}

// Part A, Q1: subscription pricing, validation and journal quotas.
static void testJournal() {
    Journal annual("J0", "Annual", "1111-2222", 4, "P", 2026, Money::of(100));
    CHECK(annual.subscriptionYears() == 1);
    CHECK(annual.costFor(2) == Money::of(200));
    CHECK(!annual.isDigital());
    CHECK(annual.issn() == "1111-2222" && annual.issuesPerYear() == 4);
    CHECK_THROWS(annual.costFor(0), std::invalid_argument);
    CHECK_THROWS(annual.costFor(-1), std::invalid_argument);
    CHECK_THROWS(Journal("J", "T", "i", 4, "P", 2026, Money::of(1), 0),
                 std::invalid_argument);
    CHECK_THROWS(Journal("J", "T", "i", 4, "P", 2026, Money::of(1), -1),
                 std::invalid_argument);
    CHECK_THROWS(Journal("J", "T", "i", 0, "P", 2026, Money::of(1)),
                 std::invalid_argument);
    Catalog c;
    c.emplace<Journal>("J1", "Journal", "1234-5678", 12, "P", 2026, Money::of(100), 3);
    const Resource& r = c.get("J1");
    CHECK(r.category() == ResourceCategory::Journal);
    CHECK(r.costFor(2) == Money::of(600));
    CHECK(c.byCategory(ResourceCategory::Journal).size() == 1);
    std::ostringstream details;
    details << r;
    CHECK(details.str().find("issn: 1234-5678") != std::string::npos);
    CHECK(details.str().find("issues per year: 12") != std::string::npos);
    CHECK(details.str().find("subscription years: 3") != std::string::npos);
    Budget b(Money::of(2000));
    b.setQuota(ResourceCategory::Journal, {2, Money::of(600)});
    AcquisitionManager acq(c, b);
    acq.purchase("J1", 2);
    CHECK(c.holdings("J1") == 2 && b.spent() == Money::of(600));
    CHECK_THROWS(acq.purchase("J1", 1), QuotaExceededError);
    CHECK(c.holdings("J1") == 2 && b.spent() == Money::of(600));
    std::ostringstream report;
    b.print(report);
    CHECK(report.str().find("Journal") != std::string::npos);
    CHECK(report.str().find("600.00/600.00") != std::string::npos);
    Budget spendLimited(Money::of(2000));
    spendLimited.setQuota(ResourceCategory::Journal, {10, Money::of(599)});
    CHECK_THROWS(spendLimited.commit(r.category(), 2, r.costFor(2)), QuotaExceededError);
    CHECK(spendLimited.spent() == Money{});
}

// Part A, Q2: inherited electronic pricing and separate e-book quotas.
static void testEBook() {
    Catalog c;
    auto& e = c.emplace<EBook>("E1", "Digital Book", std::vector<std::string>{"A", "B"},
        "isbn", "P", 2026, Money::of(10), "url", FileFormat::EPUB, true,
        LicenseModel::Perpetual, Money::of(50));
    const Resource& r = e;
    CHECK(r.isDigital() && r.category() == ResourceCategory::EBook);
    CHECK(r.costFor(3) == Money::of(80));
    CHECK(e.authors().size() == 2 && e.isbn() == "isbn");
    CHECK(e.format() == FileFormat::EPUB && e.drmProtected());
    CHECK(e.license() == LicenseModel::Perpetual && e.accessUrl() == "url");
    CHECK_THROWS(r.costFor(0), std::invalid_argument);
    CHECK_THROWS(r.costFor(-1), std::invalid_argument);
    std::ostringstream details;
    details << r;
    const auto text = details.str();
    CHECK(text.find("platform fee: 50.00") < text.find("authors: A and B"));
    CHECK(text.find("access url: url") != std::string::npos);
    CHECK(text.find("license: Perpetual") != std::string::npos);
    CHECK(text.find("isbn: isbn") != std::string::npos);
    CHECK(text.find("file format: EPUB") != std::string::npos);
    CHECK(text.find("drm protected: yes") != std::string::npos);
    for (FileFormat format : {FileFormat::PDF, FileFormat::EPUB, FileFormat::HTML}) {
        EBook other("E", "T", {}, "i", "P", 2026, Money::of(10), "url", format, false);
        CHECK(other.costFor(2) == Money::of(20));
        std::ostringstream os;
        os << other;
        CHECK(os.str().find(fileFormatName(format)) != std::string::npos);
        CHECK(os.str().find("drm protected: no") != std::string::npos);
    }
    CHECK_THROWS(EBook("E", "T", {}, "i", "P", 2026, Money::of(1), "url",
                      static_cast<FileFormat>(99), false), std::invalid_argument);
    c.emplace<ElectronicResource>("R1", "DB", "P", 2026, Money::of(10), "url");
    Budget b(Money::of(1000));
    b.setQuota(ResourceCategory::EBook, {3, Money::of(80)});
    b.setQuota(ResourceCategory::ElectronicResource, {1, Money::of(10)});
    AcquisitionManager acq(c, b);
    acq.purchase("E1", 3);
    CHECK_THROWS(acq.purchase("E1", 1), QuotaExceededError);
    acq.purchase("R1", 1); // Exhausting EBook quotas does not consume the DB quota.
    CHECK(b.usageFor(ResourceCategory::EBook).units == 3);
    CHECK(b.usageFor(ResourceCategory::ElectronicResource).units == 1);
    CHECK(c.byCategory(ResourceCategory::EBook).size() == 1);
    CHECK(c.holdings("E1") == 3);
    std::ostringstream report;
    b.print(report);
    CHECK(report.str().find("EBook") != std::string::npos);
}

// Part A, Q3: digital audiobook and print thesis through Resource interfaces.
static void testAudioBookAndThesis() {
    Catalog c;
    auto& audio = c.emplace<AudioBook>("A1", "Audio", "P", 2026, Money::of(10), "url",
                                     "Narrator", 90, LicenseModel::Perpetual, Money::of(5));
    auto& thesis = c.emplace<Thesis>("T1", "Research", "IIITD", "PhD", "Supervisor", 2026);
    CHECK(audio.narrator() == "Narrator" && audio.durationMinutes() == 90);
    CHECK(audio.category() == ResourceCategory::AudioBook && audio.isDigital());
    const Resource& a = audio;
    CHECK(a.costFor(2) == Money::of(25));
    CHECK_THROWS(a.costFor(0), std::invalid_argument);
    CHECK_THROWS(AudioBook("A", "T", "P", 2026, Money{}, "url", "N", 0),
                 std::invalid_argument);
    CHECK_THROWS(AudioBook("A", "T", "P", 2026, Money{}, "url", "N", -1),
                 std::invalid_argument);
    CHECK(thesis.category() == ResourceCategory::Thesis && !thesis.isDigital());
    CHECK(thesis.university() == "IIITD" && thesis.degree() == "PhD");
    CHECK(thesis.supervisor() == "Supervisor");
    const Resource& t = thesis;
    CHECK(t.costFor(3) == Money{});
    CHECK_THROWS(t.costFor(0), std::invalid_argument);
    Thesis paid("T2", "Paid", "U", "MSc", "S", 2026, Money::of(5));
    CHECK(paid.costFor(2) == Money::of(10));
    std::ostringstream details;
    details << a << t;
    CHECK(details.str().find("narrator: Narrator") != std::string::npos);
    CHECK(details.str().find("duration in minutes: 90") != std::string::npos);
    CHECK(details.str().find("platform fee: 5.00") != std::string::npos);
    CHECK(details.str().find("university: IIITD") != std::string::npos);
    CHECK(details.str().find("degree: PhD") != std::string::npos);
    CHECK(details.str().find("supervisor: Supervisor") != std::string::npos);
    Budget b(Money::of(100));
    b.setQuota(ResourceCategory::AudioBook, {2, Money::of(25)});
    b.setQuota(ResourceCategory::Thesis, {1, Money{}});
    AcquisitionManager acq(c, b);
    acq.purchase("A1", 2);
    acq.purchase("T1", 1);
    CHECK_THROWS(acq.purchase("T1", 1), QuotaExceededError);
    CHECK(c.holdings("A1") == 2 && c.holdings("T1") == 1);
    CHECK(b.spent() == Money::of(25));
    CHECK(c.all().size() == 2);
    std::ostringstream report;
    b.print(report);
    CHECK(report.str().find("AudioBook") != std::string::npos);
    CHECK(report.str().find("Thesis") != std::string::npos);
}

// Part B, Q4: hardcover surcharge; listed unit price stays unchanged.
static void testHardcoverPricing() {
    Book hard("H", "Hard", {}, "i", "P", 2026, Money::of(100), 1, Binding::Hardcover);
    Book paper("P", "Paper", {}, "i", "P", 2026, Money::of(100));
    const Resource& r = hard;
    CHECK(r.costFor(1) == Money::of(120));
    CHECK(r.costFor(3) == Money::of(360));
    CHECK(hard.unitPrice() == Money::of(100));
    CHECK(paper.costFor(3) == Money::of(300));
    CHECK_THROWS(hard.costFor(0), std::invalid_argument);
    CHECK_THROWS(hard.costFor(-1), std::invalid_argument);
    hard.setUnitPrice(Money::fromMinor(3));
    CHECK(hard.costFor(1) == Money::fromMinor(4)); // 3.6 rounds upward.
}

// Part B, Q5: boundaries and inherited rules for all resource types.
static void testBulkPricing() {
    Book paper("P", "Paper", {}, "i", "P", 2026, Money::of(100));
    Book hard("H", "Hard", {}, "i", "P", 2026, Money::of(100), 1, Binding::Hardcover);
    CHECK(paper.costFor(9) == Money::of(900));
    CHECK(paper.costFor(10) == Money::of(900));
    CHECK(paper.costFor(11) == Money::of(990));
    CHECK(hard.costFor(9) == Money::of(1080));
    CHECK(hard.costFor(10) == Money::of(1080));
    CHECK(hard.costFor(11) == Money::of(1188));
    Journal journal("J", "Journal", "i", 12, "P", 2026, Money::of(100), 3);
    CHECK(journal.costFor(9) == Money::of(2700));
    CHECK(journal.costFor(10) == Money::of(2700));
    Thesis thesis("T", "Thesis", "U", "PhD", "S", 2026, Money::of(10));
    CHECK(thesis.costFor(10) == Money::of(90));
    Thesis free("F", "Free", "U", "PhD", "S", 2026);
    CHECK(free.costFor(10) == Money{});
    ElectronicResource db("D", "DB", "P", 2026, Money::of(10), "url",
                          LicenseModel::Perpetual, Money::of(100));
    CHECK(db.costFor(49) == Money::of(590));
    CHECK(db.costFor(50) == Money::of(600));
    CHECK(db.costFor(51) == Money::of(605));
    CHECK(db.costFor(60) == Money::of(650));
    EBook ebook("E", "EBook", {}, "i", "P", 2026, Money::of(10), "url",
                FileFormat::PDF, false, LicenseModel::Perpetual, Money::of(100));
    AudioBook audio("A", "Audio", "P", 2026, Money::of(10), "url", "N", 90,
                    LicenseModel::Perpetual, Money::of(100));
    CHECK(ebook.costFor(51) == db.costFor(51));
    CHECK(audio.costFor(60) == db.costFor(60));
    ElectronicResource tiny("M", "Tiny", "P", 2026, Money::fromMinor(1), "url");
    CHECK(tiny.costFor(51) == Money::fromMinor(51));
    CHECK(tiny.costFor(52) == Money::fromMinor(51)); // Discounted subtotal rounded once.
    CHECK_THROWS(db.costFor(-1), std::invalid_argument);
    // Separate purchases do not combine to earn a bulk discount.
    CHECK(paper.costFor(5) + paper.costFor(5) == Money::of(1000));
}

// Part B, Q6: quote, approval and batch paths use the same post-tax amount.
static void testTaxes() {
    Catalog c;
    c.emplace<Book>("B", "Book", std::vector<std::string>{}, "i", "P", 2026, Money::of(100));
    c.emplace<ElectronicResource>("D", "DB", "P", 2026, Money::of(10), "url",
                                  LicenseModel::Perpetual, Money::of(100));
    c.emplace<Journal>("J", "Journal", "i", 12, "P", 2026, Money::of(100), 2);
    c.emplace<EBook>("E", "EBook", std::vector<std::string>{}, "i", "P", 2026,
                     Money::of(10), "url", FileFormat::PDF, false);
    c.emplace<AudioBook>("A", "Audio", "P", 2026, Money::of(10), "url", "N", 90);
    c.emplace<Thesis>("T", "Thesis", "U", "PhD", "S", 2026);
    Budget b(Money::of(5000));
    AcquisitionManager acq(c, b);
    CHECK(acq.quote("B", 1) == Money::of(100)); // Zero tax default.
    acq.setTaxRates({1000, 2000}); // 10% print; 20% electronic.
    c.emplace<Book>("H", "Hard", std::vector<std::string>{}, "i", "P", 2026,
                    Money::of(100), 1, Binding::Hardcover);
    CHECK(acq.quoteBeforeTax("H", 10) == Money::of(1080));
    CHECK(acq.quote("H", 10) == Money::of(1188));
    CHECK(acq.quoteBeforeTax("B", 10) == Money::of(900));
    CHECK(acq.quote("B", 10) == Money::of(990));
    CHECK(acq.quoteBeforeTax("D", 60) == Money::of(650));
    CHECK(acq.quote("D", 60) == Money::of(780)); // Fee is taxed too.
    CHECK(acq.quote("J", 1) == Money::of(220));
    CHECK(acq.quote("E", 1) == Money::of(12));
    CHECK(acq.quote("A", 1) == Money::of(12));
    CHECK(acq.quote("T", 1) == Money{});
    CHECK_THROWS(acq.setTaxRates({-1, 2000}), std::invalid_argument);
    CHECK(acq.taxRates().printBasisPoints == 1000);
    CHECK_THROWS(AcquisitionManager(c, b, {0, -1}), std::invalid_argument);
    b.setQuota(ResourceCategory::Book, {20, Money::of(100)});
    std::string why;
    CHECK(!acq.canPurchase("B", 1, &why)); // Pre-tax fits, post-tax does not.
    CHECK(why.find("spend quota") != std::string::npos);
    CHECK_THROWS(acq.purchase("B", 1), QuotaExceededError);
    CHECK(c.holdings("B") == 0 && b.spent() == Money{});
    auto rejected = acq.processBatch({{"B", 1}, {"missing", 1}, {"B", 0}});
    CHECK(!rejected[0].approved && rejected[0].preTaxCost == Money::of(100));
    CHECK(rejected[0].cost == Money::of(110));
    CHECK(rejected[1].preTaxCost == Money{} && rejected[2].cost == Money{});
    CHECK(b.spent() == Money{} && c.holdings("B") == 0);
    b.setQuota(ResourceCategory::Book, {20, Money::of(110)});
    const auto record = acq.purchase("B", 1);
    CHECK(record.preTaxCost == Money::of(100) && record.cost == Money::of(110));
    CHECK(b.usageFor(ResourceCategory::Book).spent == Money::of(110));
    CHECK(acq.totalBeforeTax() == Money::of(100) && acq.totalSpent() == Money::of(110));
    acq.setTaxRates({500, 750});
    CHECK(acq.quote("B", 1) == Money::of(105));
    CHECK(acq.history().back().cost == Money::of(110)); // History preserves old rate.
    std::ostringstream report;
    acq.printReport(report);
    CHECK(report.str().find("pre-tax: 100.00, post-tax: 110.00") != std::string::npos);
    CHECK(report.str().find("Total pre-tax: 100.00") != std::string::npos);
    CHECK(report.str().find("Total post-tax (spent): 110.00") != std::string::npos);
    Budget small(Money::of(109));
    AcquisitionManager limited(c, small, {1000, 0});
    CHECK(!limited.canPurchase("B", 1));
    CHECK_THROWS(limited.purchase("B", 1), BudgetExceededError);
    CHECK(small.spent() == Money{});
    Budget electronicBudget(Money::of(1000));
    electronicBudget.setQuota(ResourceCategory::ElectronicResource, {60, Money::of(650)});
    AcquisitionManager electronic(c, electronicBudget, {0, 2000});
    CHECK(!electronic.processBatch({{"D", 60}})[0].approved);
    electronicBudget.setQuota(ResourceCategory::ElectronicResource, {60, Money::of(780)});
    CHECK(electronic.processBatch({{"D", 60}})[0].approved);
    CHECK(electronicBudget.spent() == Money::of(780));
    c.emplace<Book>("tiny", "Tiny", std::vector<std::string>{}, "i", "P", 2026,
                    Money::fromMinor(5));
    acq.setTaxRates({1000, 0});
    CHECK(acq.quote("tiny", 1) == Money::fromMinor(6)); // Half a paise rounds up.
}

// Part C, Q7: count catalogue IDs, not copies or repeated purchases.
static void testTitleQuotas() {
    Catalog c;
    for (const auto& id : {"B001", "B002", "B003"})
        c.emplace<Book>(id, "Same displayed title", std::vector<std::string>{}, "i", "P",
                        2026, Money::of(10));
    c.emplace<Journal>("J001", "Journal", "issn", 12, "P", 2026, Money::of(10));
    Budget b(Money::of(1000));
    b.setQuota(ResourceCategory::Book, {30, Money::of(1000), 2});
    b.setQuota(ResourceCategory::Journal, {10, Money::of(100), 1});
    AcquisitionManager acq(c, b);
    acq.purchase("B001", 2);
    acq.purchase("B001", 1);
    CHECK(b.usageFor(ResourceCategory::Book).titles == 1);
    CHECK(*b.titlesRemaining(ResourceCategory::Book) == 1);
    CHECK(acq.canPurchase("B002", 1));
    CHECK(b.usageFor(ResourceCategory::Book).titles == 1); // Query has no effects.
    acq.purchase("B002", 1);
    CHECK(b.usageFor(ResourceCategory::Book).titles == 2);
    std::string reason;
    CHECK(!acq.canPurchase("B003", 1, &reason));
    CHECK(reason.find("title quota") != std::string::npos);
    CHECK_THROWS(acq.purchase("B003", 1), QuotaExceededError);
    CHECK(c.holdings("B003") == 0);
    CHECK(acq.canPurchase("B001", 1)); // More of an existing title is permitted.
    const auto batch = acq.processBatch({{"B003", 1}, {"B002", 1}, {"J001", 1}});
    CHECK(!batch[0].approved && batch[1].approved && batch[2].approved);
    CHECK(b.usageFor(ResourceCategory::Journal).titles == 1);
    CHECK(b.usageFor(ResourceCategory::Book).titles == 2);
    CHECK_THROWS(b.setQuota(ResourceCategory::Book, {1, Money::of(10), -1}),
                 std::invalid_argument);
    Budget zero(Money::of(100));
    zero.setQuota(ResourceCategory::Book, {10, Money::of(100), 0});
    CHECK_THROWS(zero.commit(ResourceCategory::Book, 1, Money::of(1), "B001"), QuotaExceededError);
    CHECK_THROWS(zero.commit(ResourceCategory::Book, 1, Money::of(1)), std::invalid_argument);
    CHECK(zero.spent() == Money{});
    b.removeQuota(ResourceCategory::Book);
    CHECK(!b.titlesRemaining(ResourceCategory::Book));
    acq.purchase("B003", 1);
    CHECK(b.usageFor(ResourceCategory::Book).titles == 3); // Track even without a limit.
    std::ostringstream os;
    b.print(os);
    CHECK(os.str().find("Titles used/max") != std::string::npos);
    Budget oneTitle(Money::of(1000));
    oneTitle.setQuota(ResourceCategory::Book, {20, Money::of(1000), 1});
    AcquisitionManager batchManager(c, oneTitle);
    const auto crossing = batchManager.processBatch({{"B001", 1}, {"B002", 1}, {"B001", 1}});
    CHECK(crossing[0].approved && !crossing[1].approved && crossing[2].approved);
    CHECK(oneTitle.usageFor(ResourceCategory::Book).titles == 1);
}

// Part C, Q8: refund recorded totals and preserve the original order history.
static void testCancellation() {
    Catalog c;
    c.emplace<Book>("B001", "Book", std::vector<std::string>{}, "i", "P", 2026, Money::of(100));
    c.emplace<Book>("B002", "Other", std::vector<std::string>{}, "i", "P", 2026, Money::of(100));
    c.emplace<Thesis>("T001", "Free", "U", "PhD", "S", 2026);
    Budget b(Money::of(3000));
    b.setQuota(ResourceCategory::Book, {20, Money::of(2000), 1});
    b.setQuota(ResourceCategory::Thesis, {1, Money{}, 1});
    AcquisitionManager acq(c, b, {1000, 0});
    const auto first = acq.purchase("B001", 10); // Discounted: 900 + 90 tax.
    const auto second = acq.purchase("B001", 1);
    CHECK(b.spent() == Money::of(1100));
    c.get("B001").setUnitPrice(Money::of(200));
    acq.setTaxRates({2000, 0});
    const auto cancellation = acq.cancel(first.orderNo);
    CHECK(cancellation.cancellation && cancellation.cancelledOrderNo == first.orderNo);
    CHECK(cancellation.cost == Money::of(990) && cancellation.preTaxCost == Money::of(900));
    CHECK(b.spent() == Money::of(110) && c.holdings("B001") == 1);
    CHECK(b.usageFor(ResourceCategory::Book).titles == 1); // One active order remains.
    CHECK(acq.history()[0].approved && !acq.history()[0].cancellation);
    CHECK(acq.history()[0].cost == first.cost && acq.history()[0].quantity == 10);
    CHECK(acq.totalSpent() == b.spent() && acq.totalBeforeTax() == Money::of(100));
    const auto before = acq.history().size();
    CHECK_THROWS(acq.cancel(first.orderNo), std::invalid_argument);
    CHECK_THROWS(acq.cancel(cancellation.orderNo), std::invalid_argument);
    CHECK_THROWS(acq.cancel(999), std::invalid_argument);
    CHECK(acq.history().size() == before && b.spent() == Money::of(110));
    acq.cancel(second.orderNo);
    CHECK(c.holdings("B001") == 0 && b.spent() == Money{});
    CHECK(b.usageFor(ResourceCategory::Book).titles == 0);
    CHECK(*b.unitsRemaining(ResourceCategory::Book) == 20);
    CHECK(*b.spendRemaining(ResourceCategory::Book) == Money::of(2000));
    CHECK(acq.canPurchase("B002", 1)); // Freed title slot.
    const auto free = acq.purchase("T001", 1);
    acq.cancel(free.orderNo);
    CHECK(c.holdings("T001") == 0 && b.usageFor(ResourceCategory::Thesis).titles == 0);
    auto rejected = acq.processBatch({{"missing", 1}})[0];
    CHECK_THROWS(acq.cancel(rejected.orderNo), std::invalid_argument);
    auto held = acq.purchase("B002", 2);
    c.addHoldings("B002", -1); // Simulate holdings no longer available for full cancellation.
    CHECK_THROWS(acq.cancel(held.orderNo), std::invalid_argument);
    CHECK(b.spent() == held.cost && c.holdings("B002") == 1);
    c.addHoldings("B002", 1);
    acq.cancel(held.orderNo);
    CHECK(b.spent() == Money{} && acq.totalSpent() == Money{});
    std::ostringstream os;
    acq.printReport(os);
    CHECK(os.str().find("CANCELLED") != std::string::npos);
    CHECK(os.str().find("refund of order #1") != std::string::npos);
    CHECK_THROWS(b.refund(ResourceCategory::Book, 1, Money::of(1), "B002"), std::invalid_argument);
    c.emplace<ElectronicResource>("R001", "DB", "P", 2026, Money::of(10), "url",
                                  LicenseModel::Perpetual, Money::of(100));
    acq.setTaxRates({0, 2000});
    const auto electronic = acq.purchase("R001", 60);
    CHECK(electronic.preTaxCost == Money::of(650) && electronic.cost == Money::of(780));
    acq.cancel(electronic.orderNo);
    CHECK(b.spent() == Money{} && c.holdings("R001") == 0);
    CHECK(b.usageFor(ResourceCategory::ElectronicResource).titles == 0);
    const auto removed = acq.purchase("B002", 1);
    c.remove("B002");
    CHECK_THROWS(acq.cancel(removed.orderNo), NotFoundError);
    CHECK(b.spent() == removed.cost && !acq.history().back().cancellation);
}

// Part C, Q9: independent departments, correct rejection routing and refunds.
static void testDepartments() {
    Catalog c;
    c.emplace<Book>("B001", "Book", std::vector<std::string>{}, "i", "P", 2026, Money::of(100));
    c.emplace<Book>("B002", "Other", std::vector<std::string>{}, "i", "P", 2026, Money::of(100));
    Budget defaultBudget(Money::of(1000)), cs(Money::of(500)), physics(Money::of(110));
    cs.setQuota(ResourceCategory::Book, {4, Money::of(500), 1});
    physics.setQuota(ResourceCategory::Book, {2, Money::of(500), 1});
    AcquisitionManager acq(c, defaultBudget, {1000, 0});
    acq.addDepartment("Computer Science", cs);
    acq.addDepartment("Physics", physics);
    CHECK_THROWS(acq.addDepartment("Computer Science", physics), std::invalid_argument);
    CHECK_THROWS(acq.addDepartment("", cs), std::invalid_argument);
    CHECK_THROWS(acq.addDepartment("Other", cs), std::invalid_argument);
    CHECK_THROWS(acq.addDepartment("Default", defaultBudget), std::invalid_argument);
    CHECK_THROWS(acq.departmentBudget("unknown"), std::invalid_argument);
    auto batch = acq.processBatch({{"B001", 2, "Computer Science"}, {"B001", 1, "Physics"},
                                  {"B001", 1, "Physics"}, {"B002", 1, "Computer Science"},
                                  {"B001", 1, "unknown"}});
    CHECK(batch[0].approved && batch[1].approved);
    CHECK(!batch[2].approved && batch[2].reason.find("overall budget") != std::string::npos);
    CHECK(!batch[3].approved && batch[3].reason.find("title quota") != std::string::npos);
    CHECK(!batch[4].approved && batch[4].reason.find("unknown department") != std::string::npos);
    CHECK(batch[0].department == "Computer Science" && batch[4].department == "unknown");
    CHECK(cs.spent() == Money::of(220) && physics.spent() == Money::of(110));
    CHECK(defaultBudget.spent() == Money{} && c.holdings("B001") == 3);
    std::string why;
    CHECK(!acq.canPurchase("B001", 1, &why, "unknown"));
    CHECK_THROWS(acq.purchase("B001", 1, "unknown"), std::invalid_argument);
    acq.cancel(batch[0].orderNo);
    CHECK(cs.spent() == Money{} && physics.spent() == Money::of(110));
    CHECK(c.holdings("B001") == 1);
    CHECK(acq.history().back().department == "Computer Science");
    CHECK(acq.canPurchase("B002", 1, nullptr, "Computer Science"));
    acq.purchase("B002", 1, "Computer Science");
    acq.purchase("B001", 1); // Existing API still charges the default budget.
    CHECK(defaultBudget.spent() == Money::of(110));
    CHECK(acq.totalSpent() == cs.spent() + physics.spent() + defaultBudget.spent());
    std::ostringstream report;
    acq.printReport(report);
    CHECK(report.str().find("department: Physics") != std::string::npos);
}

// Part C, Q10 and the unnumbered quota-warning requirement.
static void testRolloverAndWarnings() {
    Budget b(Money::of(1000));
    b.setQuota(ResourceCategory::Book, {10, Money::of(1000), 2});
    b.commit(ResourceCategory::Book, 8, Money::of(800), "B001");
    CHECK(b.warnings().empty()); // Exactly 80% does not exceed 80%.
    b.commit(ResourceCategory::Book, 1, Money::of(10), "B001");
    CHECK(b.warnings().size() == 2); // 90% units, 81% spend.
    std::ostringstream report;
    b.print(report);
    CHECK(report.str().find("WARNING: Book past 80% of unit quota") != std::string::npos);
    CHECK(report.str().find("WARNING: Book past 80% of spend quota") != std::string::npos);
    Budget next = b.rollover(Money::of(2000), 50);
    CHECK(next.total() == Money::of(2095)); // Half of unspent 190.
    CHECK(next.spent() == Money{} && next.usageFor(ResourceCategory::Book).units == 0);
    CHECK(next.usageFor(ResourceCategory::Book).titles == 0);
    CHECK(next.quotaFor(ResourceCategory::Book)->maxTitles == 2);
    CHECK(next.quotaFor(ResourceCategory::Book)->maxSpend == Money::of(1000));
    CHECK(next.warnings().empty());
    next.commit(ResourceCategory::Book, 1, Money::of(10), "B002");
    CHECK(b.spent() == Money::of(810) && b.usageFor(ResourceCategory::Book).titles == 1);
    CHECK(b.rollover(Money::of(2000), 0).total() == Money::of(2000));
    CHECK(b.rollover(Money::of(2000), 100).total() == Money::of(2190));
    CHECK_THROWS(b.rollover(Money::of(1), -1), std::invalid_argument);
    CHECK_THROWS(b.rollover(Money::of(1), 101), std::invalid_argument);
    CHECK_THROWS(b.rollover(Money::fromMinor(-1), 50), std::invalid_argument);
    b.refund(ResourceCategory::Book, 1, Money::of(10), "B001");
    CHECK(b.warnings().empty()); // Refund returns both usages to exactly 80%.
    Budget tiny(Money::fromMinor(5));
    CHECK(tiny.rollover(Money{}, 50).total() == Money::fromMinor(3));
    Budget noQuota(Money::of(100));
    noQuota.commit(ResourceCategory::Book, 1, Money::of(99), "B001");
    CHECK(noQuota.warnings().empty());
    Budget free(Money{});
    free.setQuota(ResourceCategory::Thesis, {1, Money{}, 1});
    free.commit(ResourceCategory::Thesis, 1, Money{}, "T001");
    CHECK(free.warnings().size() == 1); // Unit warning, no zero-limit division.
    Budget maximum(Money::fromMinor(std::numeric_limits<std::int64_t>::max()));
    CHECK_THROWS(maximum.rollover(Money::fromMinor(1), 100), std::overflow_error);
    CHECK(maximum.rollover(Money{}, 100).total() == maximum.total());
}

int main() {
    testMoney();
    testResourcesAndCost();
    testCatalog();
    testBudget();
    testAcquisition();
    testJournal();
    testEBook();
    testAudioBookAndThesis();
    testHardcoverPricing();
    testBulkPricing();
    testTaxes();
    testTitleQuotas();
    testCancellation();
    testDepartments();
    testRolloverAndWarnings();
    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
