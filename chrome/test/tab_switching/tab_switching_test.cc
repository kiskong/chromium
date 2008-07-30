// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/time.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

#define NUMBER_OF_ITERATIONS 5

namespace {

// This Automated UI test opens static files in different tabs in a proxy
// browser. After all the tabs have opened, it switches between tabs, and notes
// time taken for each switch. It then prints out the times on the console,
// with the aim that the page cycler parser can interpret these numbers to
// draw graphs for page cycler Tab Switching Performance.
// Usage Flags: -enable-logging -dump-histograms-on-exit
class TabSwitchingUITest : public UITest {
 public:
   TabSwitchingUITest() {
    PathService::Get(base::DIR_EXE, &path_prefix_);
    file_util::UpOneDirectory(&path_prefix_);
    file_util::UpOneDirectory(&path_prefix_);
    file_util::AppendToPath(&path_prefix_, L"data");
    file_util::AppendToPath(&path_prefix_, L"tab_switching");
    path_prefix_ += file_util::kPathSeparator;

    show_window_ = true;
  }

  void RunTabSwitchingUITest() {
    // Create a browser proxy.
    browser_proxy_.reset(automation()->GetBrowserWindow(0));

    // Open all the tabs.
    int initial_tab_count = 0;
    ASSERT_TRUE(browser_proxy_->GetTabCount(&initial_tab_count));
    int new_tab_count = OpenTabs();
    int final_tab_count = 0;
    ASSERT_TRUE(browser_proxy_->WaitForTabCountToChange(initial_tab_count,
                                                        &final_tab_count,
                                                        10000));
    ASSERT_TRUE(final_tab_count == initial_tab_count + new_tab_count);

    // Switch linearly between tabs.
    browser_proxy_->ActivateTab(0);
    for (int i = initial_tab_count; i < final_tab_count; ++i) {
      browser_proxy_->ActivateTab(i);
      ASSERT_TRUE(browser_proxy_->WaitForTabToBecomeActive(i, 10000));
    }

    // Close the browser to force a dump of log.
    bool application_closed = false;
    EXPECT_TRUE(CloseBrowser(browser_proxy_.get(), &application_closed));

    // Now open the corresponding log file and collect average and std dev from
    // the histogram stats generated for RenderWidgetHostHWND_WhiteoutDuration
    std::wstring log_file_name;
    PathService::Get(chrome::DIR_LOGS, &log_file_name);
    file_util::AppendToPath(&log_file_name, L"chrome_debug.log");

    bool log_has_been_dumped = false;
    std::string contents;
    do {
      log_has_been_dumped = file_util::ReadFileToString(log_file_name,
                                                        &contents);
    } while (!log_has_been_dumped);

    // Parse the contents to get average and std deviation.
    std::string average("0.0"), std_dev("0.0");
    const std::string average_str("average = ");
    const std::string std_dev_str("standard deviation = ");
    std::string::size_type pos = contents.find(
        "Histogram: MPArch.RWHH_WhiteoutDuration", 0);
    std::string::size_type comma_pos;
    std::string::size_type number_length;
    if (pos != std::string::npos) {
      // Get the average.
      pos = contents.find(average_str, pos);
      comma_pos = contents.find(",", pos);
      pos += average_str.length();
      number_length = comma_pos - pos;
      average = contents.substr(pos, number_length);

      // Get the std dev.
      pos = contents.find(std_dev_str, pos);
      pos += std_dev_str.length();
      comma_pos = contents.find(" ", pos);
      number_length = comma_pos - pos;
      std_dev = contents.substr(pos, number_length);
    }

    // Print the average and standard deviation.
    // Format: __tsw_timings = [512.00, 419.17]
    //         Where 512.00 = average
    //               419.17 = std dev.
    printf("__tsw_timings = [%s,%s]\n", average.c_str(), std_dev.c_str());
  }

 protected:
  // Opens new tabs. Returns the number of tabs opened.
  int OpenTabs() {
    // Add tabs.
    static const wchar_t* files[] = { L"espn.go.com", L"bugzilla.mozilla.org",
                                      L"news.cnet.com", L"www.amazon.com",
                                      L"kannada.chakradeo.net", L"allegro.pl",
                                      L"ml.wikipedia.org", L"www.bbc.co.uk",
                                      L"126.com", L"www.altavista.com"};
    int number_of_new_tabs_opened = 0;
    std::wstring file_name;
    for (int i = 0; i < arraysize(files); ++i) {
      file_name = path_prefix_;
      file_name += files[i];
      file_name += file_util::kPathSeparator;
      file_name += L"index.html";
      browser_proxy_->AppendTab(net_util::FilePathToFileURL(file_name));
      number_of_new_tabs_opened++;
    }

    return number_of_new_tabs_opened;
  }

  std::wstring path_prefix_;
  int number_of_tabs_to_open_;
  scoped_ptr<BrowserProxy> browser_proxy_;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(TabSwitchingUITest);
};

}  // namespace

TEST_F(TabSwitchingUITest, GenerateTabSwitchStats) {
  RunTabSwitchingUITest();
}
