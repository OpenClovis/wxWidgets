///////////////////////////////////////////////////////////////////////////////
// Name:        wx/datetime.h
// Purpose:     implementation of time/date related classes
// Author:      Vadim Zeitlin
// Modified by:
// Created:     11.05.99
// RCS-ID:      $Id$
// Copyright:   (c) 1999 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
//              parts of code taken from sndcal library by Scott E. Lee:
//
//               Copyright 1993-1995, Scott E. Lee, all rights reserved.
//               Permission granted to use, copy, modify, distribute and sell
//               so long as the above copyright and this permission statement
//               are retained in all copies.
//
// Licence:     wxWindows license
///////////////////////////////////////////////////////////////////////////////

/*
 * Implementation notes:
 *
 * 1. the time is stored as a 64bit integer containing the signed number of
 *    milliseconds since Jan 1. 1970 (the Unix Epoch) - so it is always
 *    expressed in GMT.
 *
 * 2. the range is thus something about 580 million years, but due to current
 *    algorithms limitations, only dates from Nov 24, 4714BC are handled
 *
 * 3. standard ANSI C functions are used to do time calculations whenever
 *    possible, i.e. when the date is in the range Jan 1, 1970 to 2038
 *
 * 4. otherwise, the calculations are done by converting the date to/from JDN
 *    first (the range limitation mentioned above comes from here: the
 *    algorithm used by Scott E. Lee's code only works for positive JDNs, more
 *    or less)
 *
 * 5. the object constructed for the given DD-MM-YYYY HH:MM:SS corresponds to
 *    this moment in local time and may be converted to the object
 *    corresponding to the same date/time in another time zone by using
 *    ToTimezone()
 *
 * 6. the conversions to the current (or any other) timezone are done when the
 *    internal time representation is converted to the broken-down one in
 *    wxDateTime::Tm.
 */

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __GNUG__
    #pragma implementation "datetime.h"
#endif

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if !defined(wxUSE_DATETIME) || wxUSE_DATETIME

#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/log.h"
#endif // WX_PRECOMP

#include "wx/intl.h"
#include "wx/thread.h"
#include "wx/tokenzr.h"
#include "wx/module.h"

#define wxDEFINE_TIME_CONSTANTS // before including datetime.h

#include <ctype.h>

#include "wx/datetime.h"
#include "wx/timer.h"           // for wxGetLocalTimeMillis()

// ----------------------------------------------------------------------------
// conditional compilation
// ----------------------------------------------------------------------------

#if defined(HAVE_STRPTIME) && defined(__LINUX__)
    // glibc 2.0.7 strptime() is broken - the following snippet causes it to
    // crash (instead of just failing):
    //
    //      strncpy(buf, "Tue Dec 21 20:25:40 1999", 128);
    //      strptime(buf, "%x", &tm);
    //
    // so don't use it
    #undef HAVE_STRPTIME
#endif // broken strptime()

#if !defined(WX_TIMEZONE) && !defined(WX_GMTOFF_IN_TM)
    #if defined(__BORLANDC__) || defined(__MINGW32__) || defined(__VISAGECPP__)
        #define WX_TIMEZONE _timezone
    #elif defined(__MWERKS__)
        long wxmw_timezone = 28800;
        #define WX_TIMEZONE wxmw_timezone
    #elif defined(__DJGPP__)
        #include <sys/timeb.h>
        #include <values.h>
        static long wxGetTimeZone()
        {
            static long timezone = MAXLONG; // invalid timezone
            if (timezone == MAXLONG)
            {
                struct timeb tb;
                ftime(&tb);
                timezone = tb.timezone;
            }
            return timezone;
        }
        #define WX_TIMEZONE wxGetTimeZone()
    #elif defined(__DARWIN__)
        #define WX_GMTOFF_IN_TM
    #else // unknown platform - try timezone
        #define WX_TIMEZONE timezone
    #endif
#endif // !WX_TIMEZONE && !WX_GMTOFF_IN_TM

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

// debugging helper: just a convenient replacement of wxCHECK()
#define wxDATETIME_CHECK(expr, msg)     \
        if ( !(expr) )                  \
        {                               \
            wxFAIL_MSG(msg);            \
            *this = wxInvalidDateTime;  \
            return *this;               \
        }

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

class wxDateTimeHolidaysModule : public wxModule
{
public:
    virtual bool OnInit()
    {
        wxDateTimeHolidayAuthority::AddAuthority(new wxDateTimeWorkDays);

        return TRUE;
    }

    virtual void OnExit()
    {
        wxDateTimeHolidayAuthority::ClearAllAuthorities();
        wxDateTimeHolidayAuthority::ms_authorities.Clear();
    }

private:
    DECLARE_DYNAMIC_CLASS(wxDateTimeHolidaysModule)
};

IMPLEMENT_DYNAMIC_CLASS(wxDateTimeHolidaysModule, wxModule)

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// some trivial ones
static const int MONTHS_IN_YEAR = 12;

static const int SEC_PER_MIN = 60;

static const int MIN_PER_HOUR = 60;

static const int HOURS_PER_DAY = 24;

static const long SECONDS_PER_DAY = 86400l;

static const int DAYS_PER_WEEK = 7;

static const long MILLISECONDS_PER_DAY = 86400000l;

// this is the integral part of JDN of the midnight of Jan 1, 1970
// (i.e. JDN(Jan 1, 1970) = 2440587.5)
static const long EPOCH_JDN = 2440587l;

// the date of JDN -0.5 (as we don't work with fractional parts, this is the
// reference date for us) is Nov 24, 4714BC
static const int JDN_0_YEAR = -4713;
static const int JDN_0_MONTH = wxDateTime::Nov;
static const int JDN_0_DAY = 24;

// the constants used for JDN calculations
static const long JDN_OFFSET         = 32046l;
static const long DAYS_PER_5_MONTHS  = 153l;
static const long DAYS_PER_4_YEARS   = 1461l;
static const long DAYS_PER_400_YEARS = 146097l;

// this array contains the cumulated number of days in all previous months for
// normal and leap years
static const wxDateTime::wxDateTime_t gs_cumulatedDays[2][MONTHS_IN_YEAR] =
{
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 },
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 }
};

// ----------------------------------------------------------------------------
// global data
// ----------------------------------------------------------------------------

// in the fine tradition of ANSI C we use our equivalent of (time_t)-1 to
// indicate an invalid wxDateTime object
const wxDateTime wxDefaultDateTime;

wxDateTime::Country wxDateTime::ms_country = wxDateTime::Country_Unknown;

// ----------------------------------------------------------------------------
// private globals
// ----------------------------------------------------------------------------

// a critical section is needed to protect GetTimeZone() static
// variable in MT case
#if wxUSE_THREADS
    static wxCriticalSection gs_critsectTimezone;
#endif // wxUSE_THREADS

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

// debugger helper: shows what the date really is
#ifdef __WXDEBUG__
extern const wxChar *wxDumpDate(const wxDateTime* dt)
{
    static wxChar buf[128];

    wxStrcpy(buf, dt->Format(_T("%Y-%m-%d (%a) %H:%M:%S")));

    return buf;
}
#endif // Debug

// get the number of days in the given month of the given year
static inline
wxDateTime::wxDateTime_t GetNumOfDaysInMonth(int year, wxDateTime::Month month)
{
    // the number of days in month in Julian/Gregorian calendar: the first line
    // is for normal years, the second one is for the leap ones
    static wxDateTime::wxDateTime_t daysInMonth[2][MONTHS_IN_YEAR] =
    {
        { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
        { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
    };

    return daysInMonth[wxDateTime::IsLeapYear(year)][month];
}

// returns the time zone in the C sense, i.e. the difference UTC - local
// (in seconds)
static int GetTimeZone()
{
    // set to TRUE when the timezone is set
    static bool s_timezoneSet = FALSE;
#ifdef WX_GMTOFF_IN_TM
    static long gmtoffset = LONG_MAX; // invalid timezone
#endif

    wxCRIT_SECT_LOCKER(lock, gs_critsectTimezone);

    // ensure that the timezone variable is set by calling localtime
    if ( !s_timezoneSet )
    {
        // just call localtime() instead of figuring out whether this system
        // supports tzset(), _tzset() or something else
        time_t t = 0;
        struct tm *tm;

        tm = localtime(&t);
        s_timezoneSet = TRUE;

#ifdef WX_GMTOFF_IN_TM
        // note that GMT offset is the opposite of time zone and so to return
        // consistent results in both WX_GMTOFF_IN_TM and !WX_GMTOFF_IN_TM
        // cases we have to negate it
        gmtoffset = -tm->tm_gmtoff;
#endif
    }

#ifdef WX_GMTOFF_IN_TM
    return (int)gmtoffset;
#else
    return (int)WX_TIMEZONE;
#endif
}

// return the integral part of the JDN for the midnight of the given date (to
// get the real JDN you need to add 0.5, this is, in fact, JDN of the
// noon of the previous day)
static long GetTruncatedJDN(wxDateTime::wxDateTime_t day,
                            wxDateTime::Month mon,
                            int year)
{
    // CREDIT: code below is by Scott E. Lee (but bugs are mine)

    // check the date validity
    wxASSERT_MSG(
      (year > JDN_0_YEAR) ||
      ((year == JDN_0_YEAR) && (mon > JDN_0_MONTH)) ||
      ((year == JDN_0_YEAR) && (mon == JDN_0_MONTH) && (day >= JDN_0_DAY)),
      _T("date out of range - can't convert to JDN")
                );

    // make the year positive to avoid problems with negative numbers division
    year += 4800;

    // months are counted from March here
    int month;
    if ( mon >= wxDateTime::Mar )
    {
        month = mon - 2;
    }
    else
    {
        month = mon + 10;
        year--;
    }

    // now we can simply add all the contributions together
    return ((year / 100) * DAYS_PER_400_YEARS) / 4
            + ((year % 100) * DAYS_PER_4_YEARS) / 4
            + (month * DAYS_PER_5_MONTHS + 2) / 5
            + day
            - JDN_OFFSET;
}

// this function is a wrapper around strftime(3)
static wxString CallStrftime(const wxChar *format, const tm* tm)
{
    wxChar buf[4096];
    if ( !wxStrftime(buf, WXSIZEOF(buf), format, tm) )
    {
        // buffer is too small?
        wxFAIL_MSG(_T("strftime() failed"));
    }

    return wxString(buf);
}

#ifdef HAVE_STRPTIME

// glibc2 doesn't define this in the headers unless _XOPEN_SOURCE is defined
// which, unfortunately, wreaks havoc elsewhere
#if defined(__GLIBC__) && (__GLIBC__ == 2)
    extern "C" char *strptime(const char *, const char *, struct tm *);
#endif

// Unicode-friendly strptime() wrapper
static const wxChar *
CallStrptime(const wxChar *input, const char *fmt, tm *tm)
{
    // the problem here is that strptime() returns pointer into the string we
    // passed to it while we're really interested in the pointer into the
    // original, Unicode, string so we try to transform the pointer back
#if wxUSE_UNICODE
    wxCharBuffer inputMB(wxConvertWX2MB(input));
#else // ASCII
    const char * const inputMB = input;
#endif // Unicode/Ascii

    const char *result = strptime(inputMB, fmt, tm);
    if ( !result )
        return NULL;

#if wxUSE_UNICODE
    // FIXME: this is wrong in presence of surrogates &c
    return input + (result - inputMB.data());
#else // ASCII
    return result;
#endif // Unicode/Ascii
}

#endif // HAVE_STRPTIME

// if year and/or month have invalid values, replace them with the current ones
static void ReplaceDefaultYearMonthWithCurrent(int *year,
                                               wxDateTime::Month *month)
{
    struct tm *tmNow = NULL;

    if ( *year == wxDateTime::Inv_Year )
    {
        tmNow = wxDateTime::GetTmNow();

        *year = 1900 + tmNow->tm_year;
    }

    if ( *month == wxDateTime::Inv_Month )
    {
        if ( !tmNow )
            tmNow = wxDateTime::GetTmNow();

        *month = (wxDateTime::Month)tmNow->tm_mon;
    }
}

// fll the struct tm with default values
static void InitTm(struct tm& tm)
{
    // struct tm may have etxra fields (undocumented and with unportable
    // names) which, nevertheless, must be set to 0
    memset(&tm, 0, sizeof(struct tm));

    tm.tm_mday = 1;   // mday 0 is invalid
    tm.tm_year = 76;  // any valid year
    tm.tm_isdst = -1; // auto determine
}

// parsing helpers
// ---------------

// return the month if the string is a month name or Inv_Month otherwise
static wxDateTime::Month GetMonthFromName(const wxString& name, int flags)
{
    wxDateTime::Month mon;
    for ( mon = wxDateTime::Jan; mon < wxDateTime::Inv_Month; wxNextMonth(mon) )
    {
        // case-insensitive comparison either one of or with both abbreviated
        // and not versions
        if ( flags & wxDateTime::Name_Full )
        {
            if ( name.CmpNoCase(wxDateTime::
                        GetMonthName(mon, wxDateTime::Name_Full)) == 0 )
            {
                break;
            }
        }

        if ( flags & wxDateTime::Name_Abbr )
        {
            if ( name.CmpNoCase(wxDateTime::
                        GetMonthName(mon, wxDateTime::Name_Abbr)) == 0 )
            {
                break;
            }
        }
    }

    return mon;
}

// return the weekday if the string is a weekday name or Inv_WeekDay otherwise
static wxDateTime::WeekDay GetWeekDayFromName(const wxString& name, int flags)
{
    wxDateTime::WeekDay wd;
    for ( wd = wxDateTime::Sun; wd < wxDateTime::Inv_WeekDay; wxNextWDay(wd) )
    {
        // case-insensitive comparison either one of or with both abbreviated
        // and not versions
        if ( flags & wxDateTime::Name_Full )
        {
            if ( name.CmpNoCase(wxDateTime::
                        GetWeekDayName(wd, wxDateTime::Name_Full)) == 0 )
            {
                break;
            }
        }

        if ( flags & wxDateTime::Name_Abbr )
        {
            if ( name.CmpNoCase(wxDateTime::
                        GetWeekDayName(wd, wxDateTime::Name_Abbr)) == 0 )
            {
                break;
            }
        }
    }

    return wd;
}

// scans all digits (but no more than len) and returns the resulting number
static bool GetNumericToken(size_t len, const wxChar*& p, unsigned long *number)
{
    size_t n = 1;
    wxString s;
    while ( wxIsdigit(*p) )
    {
        s += *p++;

        if ( len && ++n > len )
            break;
    }

    return !!s && s.ToULong(number);
}

// scans all alphabetic characters and returns the resulting string
static wxString GetAlphaToken(const wxChar*& p)
{
    wxString s;
    while ( wxIsalpha(*p) )
    {
        s += *p++;
    }

    return s;
}

// ============================================================================
// implementation of wxDateTime
// ============================================================================

// ----------------------------------------------------------------------------
// struct Tm
// ----------------------------------------------------------------------------

wxDateTime::Tm::Tm()
{
    year = (wxDateTime_t)wxDateTime::Inv_Year;
    mon = wxDateTime::Inv_Month;
    mday = 0;
    hour = min = sec = msec = 0;
    wday = wxDateTime::Inv_WeekDay;
}

wxDateTime::Tm::Tm(const struct tm& tm, const TimeZone& tz)
              : m_tz(tz)
{
    msec = 0;
    sec = tm.tm_sec;
    min = tm.tm_min;
    hour = tm.tm_hour;
    mday = tm.tm_mday;
    mon = (wxDateTime::Month)tm.tm_mon;
    year = 1900 + tm.tm_year;
    wday = tm.tm_wday;
    yday = tm.tm_yday;
}

bool wxDateTime::Tm::IsValid() const
{
    // we allow for the leap seconds, although we don't use them (yet)
    return (year != wxDateTime::Inv_Year) && (mon != wxDateTime::Inv_Month) &&
           (mday <= GetNumOfDaysInMonth(year, mon)) &&
           (hour < 24) && (min < 60) && (sec < 62) && (msec < 1000);
}

void wxDateTime::Tm::ComputeWeekDay()
{
    // compute the week day from day/month/year: we use the dumbest algorithm
    // possible: just compute our JDN and then use the (simple to derive)
    // formula: weekday = (JDN + 1.5) % 7
    wday = (wxDateTime::WeekDay)(GetTruncatedJDN(mday, mon, year) + 2) % 7;
}

void wxDateTime::Tm::AddMonths(int monDiff)
{
    // normalize the months field
    while ( monDiff < -mon )
    {
        year--;

        monDiff += MONTHS_IN_YEAR;
    }

    while ( monDiff + mon >= MONTHS_IN_YEAR )
    {
        year++;

        monDiff -= MONTHS_IN_YEAR;
    }

    mon = (wxDateTime::Month)(mon + monDiff);

    wxASSERT_MSG( mon >= 0 && mon < MONTHS_IN_YEAR, _T("logic error") );

    // NB: we don't check here that the resulting date is valid, this function
    //     is private and the caller must check it if needed
}

void wxDateTime::Tm::AddDays(int dayDiff)
{
    // normalize the days field
    while ( dayDiff + mday < 1 )
    {
        AddMonths(-1);

        dayDiff += GetNumOfDaysInMonth(year, mon);
    }

    mday += dayDiff;
    while ( mday > GetNumOfDaysInMonth(year, mon) )
    {
        mday -= GetNumOfDaysInMonth(year, mon);

        AddMonths(1);
    }

    wxASSERT_MSG( mday > 0 && mday <= GetNumOfDaysInMonth(year, mon),
                  _T("logic error") );
}

// ----------------------------------------------------------------------------
// class TimeZone
// ----------------------------------------------------------------------------

wxDateTime::TimeZone::TimeZone(wxDateTime::TZ tz)
{
    switch ( tz )
    {
        case wxDateTime::Local:
            // get the offset from C RTL: it returns the difference GMT-local
            // while we want to have the offset _from_ GMT, hence the '-'
            m_offset = -GetTimeZone();
            break;

        case wxDateTime::GMT_12:
        case wxDateTime::GMT_11:
        case wxDateTime::GMT_10:
        case wxDateTime::GMT_9:
        case wxDateTime::GMT_8:
        case wxDateTime::GMT_7:
        case wxDateTime::GMT_6:
        case wxDateTime::GMT_5:
        case wxDateTime::GMT_4:
        case wxDateTime::GMT_3:
        case wxDateTime::GMT_2:
        case wxDateTime::GMT_1:
            m_offset = -3600*(wxDateTime::GMT0 - tz);
            break;

        case wxDateTime::GMT0:
        case wxDateTime::GMT1:
        case wxDateTime::GMT2:
        case wxDateTime::GMT3:
        case wxDateTime::GMT4:
        case wxDateTime::GMT5:
        case wxDateTime::GMT6:
        case wxDateTime::GMT7:
        case wxDateTime::GMT8:
        case wxDateTime::GMT9:
        case wxDateTime::GMT10:
        case wxDateTime::GMT11:
        case wxDateTime::GMT12:
            m_offset = 3600*(tz - wxDateTime::GMT0);
            break;

        case wxDateTime::A_CST:
            // Central Standard Time in use in Australia = UTC + 9.5
            m_offset = 60l*(9*60 + 30);
            break;

        default:
            wxFAIL_MSG( _T("unknown time zone") );
    }
}

// ----------------------------------------------------------------------------
// static functions
// ----------------------------------------------------------------------------

/* static */
bool wxDateTime::IsLeapYear(int year, wxDateTime::Calendar cal)
{
    if ( year == Inv_Year )
        year = GetCurrentYear();

    if ( cal == Gregorian )
    {
        // in Gregorian calendar leap years are those divisible by 4 except
        // those divisible by 100 unless they're also divisible by 400
        // (in some countries, like Russia and Greece, additional corrections
        // exist, but they won't manifest themselves until 2700)
        return (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
    }
    else if ( cal == Julian )
    {
        // in Julian calendar the rule is simpler
        return year % 4 == 0;
    }
    else
    {
        wxFAIL_MSG(_T("unknown calendar"));

        return FALSE;
    }
}

/* static */
int wxDateTime::GetCentury(int year)
{
    return year > 0 ? year / 100 : year / 100 - 1;
}

/* static */
int wxDateTime::ConvertYearToBC(int year)
{
    // year 0 is BC 1
    return year > 0 ? year : year - 1;
}

/* static */
int wxDateTime::GetCurrentYear(wxDateTime::Calendar cal)
{
    switch ( cal )
    {
        case Gregorian:
            return Now().GetYear();

        case Julian:
            wxFAIL_MSG(_T("TODO"));
            break;

        default:
            wxFAIL_MSG(_T("unsupported calendar"));
            break;
    }

    return Inv_Year;
}

/* static */
wxDateTime::Month wxDateTime::GetCurrentMonth(wxDateTime::Calendar cal)
{
    switch ( cal )
    {
        case Gregorian:
            return Now().GetMonth();

        case Julian:
            wxFAIL_MSG(_T("TODO"));
            break;

        default:
            wxFAIL_MSG(_T("unsupported calendar"));
            break;
    }

    return Inv_Month;
}

/* static */
wxDateTime::wxDateTime_t wxDateTime::GetNumberOfDays(int year, Calendar cal)
{
    if ( year == Inv_Year )
    {
        // take the current year if none given
        year = GetCurrentYear();
    }

    switch ( cal )
    {
        case Gregorian:
        case Julian:
            return IsLeapYear(year) ? 366 : 365;

        default:
            wxFAIL_MSG(_T("unsupported calendar"));
            break;
    }

    return 0;
}

/* static */
wxDateTime::wxDateTime_t wxDateTime::GetNumberOfDays(wxDateTime::Month month,
                                                     int year,
                                                     wxDateTime::Calendar cal)
{
    wxCHECK_MSG( month < MONTHS_IN_YEAR, 0, _T("invalid month") );

    if ( cal == Gregorian || cal == Julian )
    {
        if ( year == Inv_Year )
        {
            // take the current year if none given
            year = GetCurrentYear();
        }

        return GetNumOfDaysInMonth(year, month);
    }
    else
    {
        wxFAIL_MSG(_T("unsupported calendar"));

        return 0;
    }
}

/* static */
wxString wxDateTime::GetMonthName(wxDateTime::Month month,
                                  wxDateTime::NameFlags flags)
{
    wxCHECK_MSG( month != Inv_Month, _T(""), _T("invalid month") );

    // notice that we must set all the fields to avoid confusing libc (GNU one
    // gets confused to a crash if we don't do this)
    tm tm;
    InitTm(tm);
    tm.tm_mon = month;

    return CallStrftime(flags == Name_Abbr ? _T("%b") : _T("%B"), &tm);
}

/* static */
wxString wxDateTime::GetWeekDayName(wxDateTime::WeekDay wday,
                                    wxDateTime::NameFlags flags)
{
    wxCHECK_MSG( wday != Inv_WeekDay, _T(""), _T("invalid weekday") );

    // take some arbitrary Sunday
    tm tm;
    InitTm(tm);
    tm.tm_mday = 28;
    tm.tm_mon = Nov;
    tm.tm_year = 99;

    // and offset it by the number of days needed to get the correct wday
    tm.tm_mday += wday;

    // call mktime() to normalize it...
    (void)mktime(&tm);

    // ... and call strftime()
    return CallStrftime(flags == Name_Abbr ? _T("%a") : _T("%A"), &tm);
}

/* static */
void wxDateTime::GetAmPmStrings(wxString *am, wxString *pm)
{
    tm tm;
    InitTm(tm);
    if ( am )
    {
        *am = CallStrftime(_T("%p"), &tm);
    }
    if ( pm )
    {
        tm.tm_hour = 13;
        *pm = CallStrftime(_T("%p"), &tm);
    }
}

// ----------------------------------------------------------------------------
// Country stuff: date calculations depend on the country (DST, work days,
// ...), so we need to know which rules to follow.
// ----------------------------------------------------------------------------

/* static */
wxDateTime::Country wxDateTime::GetCountry()
{
    // TODO use LOCALE_ICOUNTRY setting under Win32

    if ( ms_country == Country_Unknown )
    {
        // try to guess from the time zone name
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);

        wxString tz = CallStrftime(_T("%Z"), tm);
        if ( tz == _T("WET") || tz == _T("WEST") )
        {
            ms_country = UK;
        }
        else if ( tz == _T("CET") || tz == _T("CEST") )
        {
            ms_country = Country_EEC;
        }
        else if ( tz == _T("MSK") || tz == _T("MSD") )
        {
            ms_country = Russia;
        }
        else if ( tz == _T("AST") || tz == _T("ADT") ||
                  tz == _T("EST") || tz == _T("EDT") ||
                  tz == _T("CST") || tz == _T("CDT") ||
                  tz == _T("MST") || tz == _T("MDT") ||
                  tz == _T("PST") || tz == _T("PDT") )
        {
            ms_country = USA;
        }
        else
        {
            // well, choose a default one
            ms_country = USA;
        }
    }

    return ms_country;
}

/* static */
void wxDateTime::SetCountry(wxDateTime::Country country)
{
    ms_country = country;
}

/* static */
bool wxDateTime::IsWestEuropeanCountry(Country country)
{
    if ( country == Country_Default )
    {
        country = GetCountry();
    }

    return (Country_WesternEurope_Start <= country) &&
           (country <= Country_WesternEurope_End);
}

// ----------------------------------------------------------------------------
// DST calculations: we use 3 different rules for the West European countries,
// USA and for the rest of the world. This is undoubtedly false for many
// countries, but I lack the necessary info (and the time to gather it),
// please add the other rules here!
// ----------------------------------------------------------------------------

/* static */
bool wxDateTime::IsDSTApplicable(int year, Country country)
{
    if ( year == Inv_Year )
    {
        // take the current year if none given
        year = GetCurrentYear();
    }

    if ( country == Country_Default )
    {
        country = GetCountry();
    }

    switch ( country )
    {
        case USA:
        case UK:
            // DST was first observed in the US and UK during WWI, reused
            // during WWII and used again since 1966
            return year >= 1966 ||
                   (year >= 1942 && year <= 1945) ||
                   (year == 1918 || year == 1919);

        default:
            // assume that it started after WWII
            return year > 1950;
    }
}

/* static */
wxDateTime wxDateTime::GetBeginDST(int year, Country country)
{
    if ( year == Inv_Year )
    {
        // take the current year if none given
        year = GetCurrentYear();
    }

    if ( country == Country_Default )
    {
        country = GetCountry();
    }

    if ( !IsDSTApplicable(year, country) )
    {
        return wxInvalidDateTime;
    }

    wxDateTime dt;

    if ( IsWestEuropeanCountry(country) || (country == Russia) )
    {
        // DST begins at 1 a.m. GMT on the last Sunday of March
        if ( !dt.SetToLastWeekDay(Sun, Mar, year) )
        {
            // weird...
            wxFAIL_MSG( _T("no last Sunday in March?") );
        }

        dt += wxTimeSpan::Hours(1);

        // disable DST tests because it could result in an infinite recursion!
        dt.MakeGMT(TRUE);
    }
    else switch ( country )
    {
        case USA:
            switch ( year )
            {
                case 1918:
                case 1919:
                    // don't know for sure - assume it was in effect all year

                case 1943:
                case 1944:
                case 1945:
                    dt.Set(1, Jan, year);
                    break;

                case 1942:
                    // DST was installed Feb 2, 1942 by the Congress
                    dt.Set(2, Feb, year);
                    break;

                    // Oil embargo changed the DST period in the US
                case 1974:
                    dt.Set(6, Jan, 1974);
                    break;

                case 1975:
                    dt.Set(23, Feb, 1975);
                    break;

                default:
                    // before 1986, DST begun on the last Sunday of April, but
                    // in 1986 Reagan changed it to begin at 2 a.m. of the
                    // first Sunday in April
                    if ( year < 1986 )
                    {
                        if ( !dt.SetToLastWeekDay(Sun, Apr, year) )
                        {
                            // weird...
                            wxFAIL_MSG( _T("no first Sunday in April?") );
                        }
                    }
                    else
                    {
                        if ( !dt.SetToWeekDay(Sun, 1, Apr, year) )
                        {
                            // weird...
                            wxFAIL_MSG( _T("no first Sunday in April?") );
                        }
                    }

                    dt += wxTimeSpan::Hours(2);

                    // TODO what about timezone??
            }

            break;

        default:
            // assume Mar 30 as the start of the DST for the rest of the world
            // - totally bogus, of course
            dt.Set(30, Mar, year);
    }

    return dt;
}

/* static */
wxDateTime wxDateTime::GetEndDST(int year, Country country)
{
    if ( year == Inv_Year )
    {
        // take the current year if none given
        year = GetCurrentYear();
    }

    if ( country == Country_Default )
    {
        country = GetCountry();
    }

    if ( !IsDSTApplicable(year, country) )
    {
        return wxInvalidDateTime;
    }

    wxDateTime dt;

    if ( IsWestEuropeanCountry(country) || (country == Russia) )
    {
        // DST ends at 1 a.m. GMT on the last Sunday of October
        if ( !dt.SetToLastWeekDay(Sun, Oct, year) )
        {
            // weirder and weirder...
            wxFAIL_MSG( _T("no last Sunday in October?") );
        }

        dt += wxTimeSpan::Hours(1);

        // disable DST tests because it could result in an infinite recursion!
        dt.MakeGMT(TRUE);
    }
    else switch ( country )
    {
        case USA:
            switch ( year )
            {
                case 1918:
                case 1919:
                    // don't know for sure - assume it was in effect all year

                case 1943:
                case 1944:
                    dt.Set(31, Dec, year);
                    break;

                case 1945:
                    // the time was reset after the end of the WWII
                    dt.Set(30, Sep, year);
                    break;

                default:
                    // DST ends at 2 a.m. on the last Sunday of October
                    if ( !dt.SetToLastWeekDay(Sun, Oct, year) )
                    {
                        // weirder and weirder...
                        wxFAIL_MSG( _T("no last Sunday in October?") );
                    }

                    dt += wxTimeSpan::Hours(2);

                    // TODO what about timezone??
            }
            break;

        default:
            // assume October 26th as the end of the DST - totally bogus too
            dt.Set(26, Oct, year);
    }

    return dt;
}

// ----------------------------------------------------------------------------
// constructors and assignment operators
// ----------------------------------------------------------------------------

// return the current time with ms precision
/* static */ wxDateTime wxDateTime::UNow()
{
    return wxDateTime(wxGetLocalTimeMillis());
}

// the values in the tm structure contain the local time
wxDateTime& wxDateTime::Set(const struct tm& tm)
{
    struct tm tm2(tm);
    time_t timet = mktime(&tm2);

    if ( timet == (time_t)-1 )
    {
        // mktime() rather unintuitively fails for Jan 1, 1970 if the hour is
        // less than timezone - try to make it work for this case
        if ( tm2.tm_year == 70 && tm2.tm_mon == 0 && tm2.tm_mday == 1 )
        {
            // add timezone to make sure that date is in range
            tm2.tm_sec -= GetTimeZone();

            timet = mktime(&tm2);
            if ( timet != (time_t)-1 )
            {
                timet += GetTimeZone();

                return Set(timet);
            }
        }

        wxFAIL_MSG( _T("mktime() failed") );

        *this = wxInvalidDateTime;

        return *this;
    }
    else
    {
        return Set(timet);
    }
}

wxDateTime& wxDateTime::Set(wxDateTime_t hour,
                            wxDateTime_t minute,
                            wxDateTime_t second,
                            wxDateTime_t millisec)
{
    // we allow seconds to be 61 to account for the leap seconds, even if we
    // don't use them really
    wxDATETIME_CHECK( hour < 24 &&
                      second < 62 &&
                      minute < 60 &&
                      millisec < 1000,
                      _T("Invalid time in wxDateTime::Set()") );

    // get the current date from system
    struct tm *tm = GetTmNow();

    wxDATETIME_CHECK( tm, _T("localtime() failed") );

    // adjust the time
    tm->tm_hour = hour;
    tm->tm_min = minute;
    tm->tm_sec = second;

    (void)Set(*tm);

    // and finally adjust milliseconds
    return SetMillisecond(millisec);
}

wxDateTime& wxDateTime::Set(wxDateTime_t day,
                            Month        month,
                            int          year,
                            wxDateTime_t hour,
                            wxDateTime_t minute,
                            wxDateTime_t second,
                            wxDateTime_t millisec)
{
    wxDATETIME_CHECK( hour < 24 &&
                      second < 62 &&
                      minute < 60 &&
                      millisec < 1000,
                      _T("Invalid time in wxDateTime::Set()") );

    ReplaceDefaultYearMonthWithCurrent(&year, &month);

    wxDATETIME_CHECK( (0 < day) && (day <= GetNumberOfDays(month, year)),
                      _T("Invalid date in wxDateTime::Set()") );

    // the range of time_t type (inclusive)
    static const int yearMinInRange = 1970;
    static const int yearMaxInRange = 2037;

    // test only the year instead of testing for the exact end of the Unix
    // time_t range - it doesn't bring anything to do more precise checks
    if ( year >= yearMinInRange && year <= yearMaxInRange )
    {
        // use the standard library version if the date is in range - this is
        // probably more efficient than our code
        struct tm tm;
        tm.tm_year = year - 1900;
        tm.tm_mon = month;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
        tm.tm_isdst = -1;       // mktime() will guess it

        (void)Set(tm);

        // and finally adjust milliseconds
        return SetMillisecond(millisec);
    }
    else
    {
        // do time calculations ourselves: we want to calculate the number of
        // milliseconds between the given date and the epoch

        // get the JDN for the midnight of this day
        m_time = GetTruncatedJDN(day, month, year);
        m_time -= EPOCH_JDN;
        m_time *= SECONDS_PER_DAY * TIME_T_FACTOR;

        // JDN corresponds to GMT, we take localtime
        Add(wxTimeSpan(hour, minute, second + GetTimeZone(), millisec));
    }

    return *this;
}

wxDateTime& wxDateTime::Set(double jdn)
{
    // so that m_time will be 0 for the midnight of Jan 1, 1970 which is jdn
    // EPOCH_JDN + 0.5
    jdn -= EPOCH_JDN + 0.5;

    jdn *= MILLISECONDS_PER_DAY;

    m_time.Assign(jdn);

    return *this;
}

wxDateTime& wxDateTime::ResetTime()
{
    Tm tm = GetTm();

    if ( tm.hour || tm.min || tm.sec || tm.msec )
    {
        tm.msec =
        tm.sec =
        tm.min =
        tm.hour = 0;

        Set(tm);
    }

    return *this;
}

// ----------------------------------------------------------------------------
// DOS Date and Time Format functions
// ----------------------------------------------------------------------------
// the dos date and time value is an unsigned 32 bit value in the format:
// YYYYYYYMMMMDDDDDhhhhhmmmmmmsssss
//
// Y = year offset from 1980 (0-127)
// M = month (1-12)
// D = day of month (1-31)
// h = hour (0-23)
// m = minute (0-59)
// s = bisecond (0-29) each bisecond indicates two seconds
// ----------------------------------------------------------------------------

wxDateTime& wxDateTime::SetFromDOS(unsigned long ddt)
{
    struct tm tm;
    InitTm(tm);

    long year = ddt & 0xFE000000;
    year >>= 25;
    year += 80;
    tm.tm_year = year;

    long month = ddt & 0x1E00000;
    month >>= 21;
    month -= 1;
    tm.tm_mon = month;

    long day = ddt & 0x1F0000;
    day >>= 16;
    tm.tm_mday = day;

    long hour = ddt & 0xF800;
    hour >>= 11;
    tm.tm_hour = hour;

    long minute = ddt & 0x7E0;
    minute >>= 5;
    tm.tm_min = minute;

    long second = ddt & 0x1F;
    tm.tm_sec = second * 2;

    return Set(mktime(&tm));
}

unsigned long wxDateTime::GetAsDOS() const
{
    unsigned long ddt;
    time_t ticks = GetTicks();
    struct tm *tm = localtime(&ticks);

    long year = tm->tm_year;
    year -= 80;
    year <<= 25;

    long month = tm->tm_mon;
    month += 1;
    month <<= 21;

    long day = tm->tm_mday;
    day <<= 16;

    long hour = tm->tm_hour;
    hour <<= 11;

    long minute = tm->tm_min;
    minute <<= 5;

    long second = tm->tm_sec;
    second /= 2;

    ddt = year | month | day | hour | minute | second;
    return ddt;
}

// ----------------------------------------------------------------------------
// time_t <-> broken down time conversions
// ----------------------------------------------------------------------------

wxDateTime::Tm wxDateTime::GetTm(const TimeZone& tz) const
{
    wxASSERT_MSG( IsValid(), _T("invalid wxDateTime") );

    time_t time = GetTicks();
    if ( time != (time_t)-1 )
    {
        // use C RTL functions
        tm *tm;
        if ( tz.GetOffset() == -GetTimeZone() )
        {
            // we are working with local time
            tm = localtime(&time);

            // should never happen
            wxCHECK_MSG( tm, Tm(), _T("localtime() failed") );
        }
        else
        {
            time += (time_t)tz.GetOffset();
#if defined(__VMS__) || defined(__WATCOMC__) // time is unsigned so avoid warning
            int time2 = (int) time;
            if ( time2 >= 0 )
#else
            if ( time >= 0 )
#endif
            {
                tm = gmtime(&time);

                // should never happen
                wxCHECK_MSG( tm, Tm(), _T("gmtime() failed") );
            }
            else
            {
                tm = (struct tm *)NULL;
            }
        }

        if ( tm )
        {
            // adjust the milliseconds
            Tm tm2(*tm, tz);
            long timeOnly = (m_time % MILLISECONDS_PER_DAY).ToLong();
            tm2.msec = (wxDateTime_t)(timeOnly % 1000);
            return tm2;
        }
        //else: use generic code below
    }

    // remember the time and do the calculations with the date only - this
    // eliminates rounding errors of the floating point arithmetics

    wxLongLong timeMidnight = m_time + tz.GetOffset() * 1000;

    long timeOnly = (timeMidnight % MILLISECONDS_PER_DAY).ToLong();

    // we want to always have positive time and timeMidnight to be really
    // the midnight before it
    if ( timeOnly < 0 )
    {
        timeOnly = MILLISECONDS_PER_DAY + timeOnly;
    }

    timeMidnight -= timeOnly;

    // calculate the Gregorian date from JDN for the midnight of our date:
    // this will yield day, month (in 1..12 range) and year

    // actually, this is the JDN for the noon of the previous day
    long jdn = (timeMidnight / MILLISECONDS_PER_DAY).ToLong() + EPOCH_JDN;

    // CREDIT: code below is by Scott E. Lee (but bugs are mine)

    wxASSERT_MSG( jdn > -2, _T("JDN out of range") );

    // calculate the century
    long temp = (jdn + JDN_OFFSET) * 4 - 1;
    long century = temp / DAYS_PER_400_YEARS;

    // then the year and day of year (1 <= dayOfYear <= 366)
    temp = ((temp % DAYS_PER_400_YEARS) / 4) * 4 + 3;
    long year = (century * 100) + (temp / DAYS_PER_4_YEARS);
    long dayOfYear = (temp % DAYS_PER_4_YEARS) / 4 + 1;

    // and finally the month and day of the month
    temp = dayOfYear * 5 - 3;
    long month = temp / DAYS_PER_5_MONTHS;
    long day = (temp % DAYS_PER_5_MONTHS) / 5 + 1;

    // month is counted from March - convert to normal
    if ( month < 10 )
    {
        month += 3;
    }
    else
    {
        year += 1;
        month -= 9;
    }

    // year is offset by 4800
    year -= 4800;

    // check that the algorithm gave us something reasonable
    wxASSERT_MSG( (0 < month) && (month <= 12), _T("invalid month") );
    wxASSERT_MSG( (1 <= day) && (day < 32), _T("invalid day") );

    // construct Tm from these values
    Tm tm;
    tm.year = (int)year;
    tm.mon = (Month)(month - 1); // algorithm yields 1 for January, not 0
    tm.mday = (wxDateTime_t)day;
    tm.msec = (wxDateTime_t)(timeOnly % 1000);
    timeOnly -= tm.msec;
    timeOnly /= 1000;               // now we have time in seconds

    tm.sec = (wxDateTime_t)(timeOnly % 60);
    timeOnly -= tm.sec;
    timeOnly /= 60;                 // now we have time in minutes

    tm.min = (wxDateTime_t)(timeOnly % 60);
    timeOnly -= tm.min;

    tm.hour = (wxDateTime_t)(timeOnly / 60);

    return tm;
}

wxDateTime& wxDateTime::SetYear(int year)
{
    wxASSERT_MSG( IsValid(), _T("invalid wxDateTime") );

    Tm tm(GetTm());
    tm.year = year;
    Set(tm);

    return *this;
}

wxDateTime& wxDateTime::SetMonth(Month month)
{
    wxASSERT_MSG( IsValid(), _T("invalid wxDateTime") );

    Tm tm(GetTm());
    tm.mon = month;
    Set(tm);

    return *this;
}

wxDateTime& wxDateTime::SetDay(wxDateTime_t mday)
{
    wxASSERT_MSG( IsValid(), _T("invalid wxDateTime") );

    Tm tm(GetTm());
    tm.mday = mday;
    Set(tm);

    return *this;
}

wxDateTime& wxDateTime::SetHour(wxDateTime_t hour)
{
    wxASSERT_MSG( IsValid(), _T("invalid wxDateTime") );

    Tm tm(GetTm());
    tm.hour = hour;
    Set(tm);

    return *this;
}

wxDateTime& wxDateTime::SetMinute(wxDateTime_t min)
{
    wxASSERT_MSG( IsValid(), _T("invalid wxDateTime") );

    Tm tm(GetTm());
    tm.min = min;
    Set(tm);

    return *this;
}

wxDateTime& wxDateTime::SetSecond(wxDateTime_t sec)
{
    wxASSERT_MSG( IsValid(), _T("invalid wxDateTime") );

    Tm tm(GetTm());
    tm.sec = sec;
    Set(tm);

    return *this;
}

wxDateTime& wxDateTime::SetMillisecond(wxDateTime_t millisecond)
{
    wxASSERT_MSG( IsValid(), _T("invalid wxDateTime") );

    // we don't need to use GetTm() for this one
    m_time -= m_time % 1000l;
    m_time += millisecond;

    return *this;
}

// ----------------------------------------------------------------------------
// wxDateTime arithmetics
// ----------------------------------------------------------------------------

wxDateTime& wxDateTime::Add(const wxDateSpan& diff)
{
    Tm tm(GetTm());

    tm.year += diff.GetYears();
    tm.AddMonths(diff.GetMonths());

    // check that the resulting date is valid
    if ( tm.mday > GetNumOfDaysInMonth(tm.year, tm.mon) )
    {
        // We suppose that when adding one month to Jan 31 we want to get Feb
        // 28 (or 29), i.e. adding a month to the last day of the month should
        // give the last day of the next month which is quite logical.
        //
        // Unfortunately, there is no logic way to understand what should
        // Jan 30 + 1 month be - Feb 28 too or Feb 27 (assuming non leap year)?
        // We make it Feb 28 (last day too), but it is highly questionable.
        tm.mday = GetNumOfDaysInMonth(tm.year, tm.mon);
    }

    tm.AddDays(diff.GetTotalDays());

    Set(tm);

    wxASSERT_MSG( IsSameTime(tm),
                  _T("Add(wxDateSpan) shouldn't modify time") );

    return *this;
}

// ----------------------------------------------------------------------------
// Weekday and monthday stuff
// ----------------------------------------------------------------------------

bool wxDateTime::SetToTheWeek(wxDateTime_t numWeek,
                              WeekDay weekday,
                              WeekFlags flags)
{
    wxASSERT_MSG( numWeek > 0,
                  _T("invalid week number: weeks are counted from 1") );

    int year = GetYear();

    // Jan 4 always lies in the 1st week of the year
    Set(4, Jan, year);
    SetToWeekDayInSameWeek(weekday, flags) += wxDateSpan::Weeks(numWeek - 1);

    if ( GetYear() != year )
    {
        // oops... numWeek was too big
        return FALSE;
    }

    return TRUE;
}

wxDateTime& wxDateTime::SetToLastMonthDay(Month month,
                                          int year)
{
    // take the current month/year if none specified
    if ( year == Inv_Year )
        year = GetYear();
    if ( month == Inv_Month )
        month = GetMonth();

    return Set(GetNumOfDaysInMonth(year, month), month, year);
}

wxDateTime& wxDateTime::SetToWeekDayInSameWeek(WeekDay weekday, WeekFlags flags)
{
    wxDATETIME_CHECK( weekday != Inv_WeekDay, _T("invalid weekday") );

    int wdayThis = GetWeekDay();
    if ( weekday == wdayThis )
    {
        // nothing to do
        return *this;
    }

    if ( flags == Default_First )
    {
        flags = GetCountry() == USA ? Sunday_First : Monday_First;
    }

    // the logic below based on comparing weekday and wdayThis works if Sun (0)
    // is the first day in the week, but breaks down for Monday_First case so
    // we adjust the week days in this case
    if( flags == Monday_First )
    {
        if ( wdayThis == Sun )
            wdayThis += 7;
    }
    //else: Sunday_First, nothing to do

    // go forward or back in time to the day we want
    if ( weekday < wdayThis )
    {
        return Subtract(wxDateSpan::Days(wdayThis - weekday));
    }
    else // weekday > wdayThis
    {
        return Add(wxDateSpan::Days(weekday - wdayThis));
    }
}

wxDateTime& wxDateTime::SetToNextWeekDay(WeekDay weekday)
{
    wxDATETIME_CHECK( weekday != Inv_WeekDay, _T("invalid weekday") );

    int diff;
    WeekDay wdayThis = GetWeekDay();
    if ( weekday == wdayThis )
    {
        // nothing to do
        return *this;
    }
    else if ( weekday < wdayThis )
    {
        // need to advance a week
        diff = 7 - (wdayThis - weekday);
    }
    else // weekday > wdayThis
    {
        diff = weekday - wdayThis;
    }

    return Add(wxDateSpan::Days(diff));
}

wxDateTime& wxDateTime::SetToPrevWeekDay(WeekDay weekday)
{
    wxDATETIME_CHECK( weekday != Inv_WeekDay, _T("invalid weekday") );

    int diff;
    WeekDay wdayThis = GetWeekDay();
    if ( weekday == wdayThis )
    {
        // nothing to do
        return *this;
    }
    else if ( weekday > wdayThis )
    {
        // need to go to previous week
        diff = 7 - (weekday - wdayThis);
    }
    else // weekday < wdayThis
    {
        diff = wdayThis - weekday;
    }

    return Subtract(wxDateSpan::Days(diff));
}

bool wxDateTime::SetToWeekDay(WeekDay weekday,
                              int n,
                              Month month,
                              int year)
{
    wxCHECK_MSG( weekday != Inv_WeekDay, FALSE, _T("invalid weekday") );

    // we don't check explicitly that -5 <= n <= 5 because we will return FALSE
    // anyhow in such case - but may be should still give an assert for it?

    // take the current month/year if none specified
    ReplaceDefaultYearMonthWithCurrent(&year, &month);

    wxDateTime dt;

    // TODO this probably could be optimised somehow...

    if ( n > 0 )
    {
        // get the first day of the month
        dt.Set(1, month, year);

        // get its wday
        WeekDay wdayFirst = dt.GetWeekDay();

        // go to the first weekday of the month
        int diff = weekday - wdayFirst;
        if ( diff < 0 )
            diff += 7;

        // add advance n-1 weeks more
        diff += 7*(n - 1);

        dt += wxDateSpan::Days(diff);
    }
    else // count from the end of the month
    {
        // get the last day of the month
        dt.SetToLastMonthDay(month, year);

        // get its wday
        WeekDay wdayLast = dt.GetWeekDay();

        // go to the last weekday of the month
        int diff = wdayLast - weekday;
        if ( diff < 0 )
            diff += 7;

        // and rewind n-1 weeks from there
        diff += 7*(-n - 1);

        dt -= wxDateSpan::Days(diff);
    }

    // check that it is still in the same month
    if ( dt.GetMonth() == month )
    {
        *this = dt;

        return TRUE;
    }
    else
    {
        // no such day in this month
        return FALSE;
    }
}

wxDateTime::wxDateTime_t wxDateTime::GetDayOfYear(const TimeZone& tz) const
{
    Tm tm(GetTm(tz));

    return gs_cumulatedDays[IsLeapYear(tm.year)][tm.mon] + tm.mday;
}

wxDateTime::wxDateTime_t wxDateTime::GetWeekOfYear(wxDateTime::WeekFlags flags,
                                                   const TimeZone& tz) const
{
    if ( flags == Default_First )
    {
        flags = GetCountry() == USA ? Sunday_First : Monday_First;
    }

    wxDateTime_t nDayInYear = GetDayOfYear(tz);
    wxDateTime_t week;

    WeekDay wd = GetWeekDay(tz);
    if ( flags == Sunday_First )
    {
        week = (nDayInYear - wd + 7) / 7;
    }
    else
    {
        // have to shift the week days values
        week = (nDayInYear - (wd - 1 + 7) % 7 + 7) / 7;
    }

    // FIXME some more elegant way??
    WeekDay wdYearStart = wxDateTime(1, Jan, GetYear()).GetWeekDay();
    if ( wdYearStart == Wed || wdYearStart == Thu )
    {
        week++;
    }

    return week;
}

wxDateTime::wxDateTime_t wxDateTime::GetWeekOfMonth(wxDateTime::WeekFlags flags,
                                                    const TimeZone& tz) const
{
    Tm tm = GetTm(tz);
    wxDateTime dtMonthStart = wxDateTime(1, tm.mon, tm.year);
    int nWeek = GetWeekOfYear(flags) - dtMonthStart.GetWeekOfYear(flags) + 1;
    if ( nWeek < 0 )
    {
        // this may happen for January when Jan, 1 is the last week of the
        // previous year
        nWeek += IsLeapYear(tm.year - 1) ? 53 : 52;
    }

    return (wxDateTime::wxDateTime_t)nWeek;
}

wxDateTime& wxDateTime::SetToYearDay(wxDateTime::wxDateTime_t yday)
{
    int year = GetYear();
    wxDATETIME_CHECK( (0 < yday) && (yday <= GetNumberOfDays(year)),
                      _T("invalid year day") );

    bool isLeap = IsLeapYear(year);
    for ( Month mon = Jan; mon < Inv_Month; wxNextMonth(mon) )
    {
        // for Dec, we can't compare with gs_cumulatedDays[mon + 1], but we
        // don't need it neither - because of the CHECK above we know that
        // yday lies in December then
        if ( (mon == Dec) || (yday < gs_cumulatedDays[isLeap][mon + 1]) )
        {
            Set(yday - gs_cumulatedDays[isLeap][mon], mon, year);

            break;
        }
    }

    return *this;
}

// ----------------------------------------------------------------------------
// Julian day number conversion and related stuff
// ----------------------------------------------------------------------------

double wxDateTime::GetJulianDayNumber() const
{
    // JDN are always expressed for the GMT dates
    Tm tm(ToTimezone(GMT0).GetTm(GMT0));

    double result = GetTruncatedJDN(tm.mday, tm.mon, tm.year);

    // add the part GetTruncatedJDN() neglected
    result += 0.5;

    // and now add the time: 86400 sec = 1 JDN
    return result + ((double)(60*(60*tm.hour + tm.min) + tm.sec)) / 86400;
}

double wxDateTime::GetRataDie() const
{
    // March 1 of the year 0 is Rata Die day -306 and JDN 1721119.5
    return GetJulianDayNumber() - 1721119.5 - 306;
}

// ----------------------------------------------------------------------------
// timezone and DST stuff
// ----------------------------------------------------------------------------

int wxDateTime::IsDST(wxDateTime::Country country) const
{
    wxCHECK_MSG( country == Country_Default, -1,
                 _T("country support not implemented") );

    // use the C RTL for the dates in the standard range
    time_t timet = GetTicks();
    if ( timet != (time_t)-1 )
    {
        tm *tm = localtime(&timet);

        wxCHECK_MSG( tm, -1, _T("localtime() failed") );

        return tm->tm_isdst;
    }
    else
    {
        int year = GetYear();

        if ( !IsDSTApplicable(year, country) )
        {
            // no DST time in this year in this country
            return -1;
        }

        return IsBetween(GetBeginDST(year, country), GetEndDST(year, country));
    }
}

wxDateTime& wxDateTime::MakeTimezone(const TimeZone& tz, bool noDST)
{
    long secDiff = GetTimeZone() + tz.GetOffset();

    // we need to know whether DST is or not in effect for this date unless
    // the test disabled by the caller
    if ( !noDST && (IsDST() == 1) )
    {
        // FIXME we assume that the DST is always shifted by 1 hour
        secDiff -= 3600;
    }

    return Subtract(wxTimeSpan::Seconds(secDiff));
}

// ----------------------------------------------------------------------------
// wxDateTime to/from text representations
// ----------------------------------------------------------------------------

wxString wxDateTime::Format(const wxChar *format, const TimeZone& tz) const
{
    wxCHECK_MSG( format, _T(""), _T("NULL format in wxDateTime::Format") );

    // we have to use our own implementation if the date is out of range of
    // strftime() or if we use non standard specificators
    time_t time = GetTicks();
    if ( (time != (time_t)-1) && !wxStrstr(format, _T("%l")) )
    {
        // use strftime()
        tm *tm;
        if ( tz.GetOffset() == -GetTimeZone() )
        {
            // we are working with local time
            tm = localtime(&time);

            // should never happen
            wxCHECK_MSG( tm, wxEmptyString, _T("localtime() failed") );
        }
        else
        {
            time += (int)tz.GetOffset();

#if defined(__VMS__) || defined(__WATCOMC__) // time is unsigned so avoid warning
            int time2 = (int) time;
            if ( time2 >= 0 )
#else
            if ( time >= 0 )
#endif
            {
                tm = gmtime(&time);

                // should never happen
                wxCHECK_MSG( tm, wxEmptyString, _T("gmtime() failed") );
            }
            else
            {
                tm = (struct tm *)NULL;
            }
        }

        if ( tm )
        {
            return CallStrftime(format, tm);
        }
        //else: use generic code below
    }

    // we only parse ANSI C format specifications here, no POSIX 2
    // complications, no GNU extensions but we do add support for a "%l" format
    // specifier allowing to get the number of milliseconds
    Tm tm = GetTm(tz);

    // used for calls to strftime() when we only deal with time
    struct tm tmTimeOnly;
    tmTimeOnly.tm_hour = tm.hour;
    tmTimeOnly.tm_min = tm.min;
    tmTimeOnly.tm_sec = tm.sec;
    tmTimeOnly.tm_wday = 0;
    tmTimeOnly.tm_yday = 0;
    tmTimeOnly.tm_mday = 1;         // any date will do
    tmTimeOnly.tm_mon = 0;
    tmTimeOnly.tm_year = 76;
    tmTimeOnly.tm_isdst = 0;        // no DST, we adjust for tz ourselves

    wxString tmp, res, fmt;
    for ( const wxChar *p = format; *p; p++ )
    {
        if ( *p != _T('%') )
        {
            // copy as is
            res += *p;

            continue;
        }

        // set the default format
        switch ( *++p )
        {
            case _T('Y'):               // year has 4 digits
                fmt = _T("%04d");
                break;

            case _T('j'):               // day of year has 3 digits
            case _T('l'):               // milliseconds have 3 digits
                fmt = _T("%03d");
                break;

            case _T('w'):               // week day as number has only one
                fmt = _T("%d");
                break;

            default:
                // it's either another valid format specifier in which case
                // the format is "%02d" (for all the rest) or we have the
                // field width preceding the format in which case it will
                // override the default format anyhow
                fmt = _T("%02d");
        }

        bool restart = TRUE;
        while ( restart )
        {
            restart = FALSE;

            // start of the format specification
            switch ( *p )
            {
                case _T('a'):       // a weekday name
                case _T('A'):
                    // second parameter should be TRUE for abbreviated names
                    res += GetWeekDayName(tm.GetWeekDay(),
                                          *p == _T('a') ? Name_Abbr : Name_Full);
                    break;

                case _T('b'):       // a month name
                case _T('B'):
                    res += GetMonthName(tm.mon,
                                        *p == _T('b') ? Name_Abbr : Name_Full);
                    break;

                case _T('c'):       // locale default date and time  representation
                case _T('x'):       // locale default date representation
                    //
                    // the problem: there is no way to know what do these format
                    // specifications correspond to for the current locale.
                    //
                    // the solution: use a hack and still use strftime(): first
                    // find the YEAR which is a year in the strftime() range (1970
                    // - 2038) whose Jan 1 falls on the same week day as the Jan 1
                    // of the real year. Then make a copy of the format and
                    // replace all occurences of YEAR in it with some unique
                    // string not appearing anywhere else in it, then use
                    // strftime() to format the date in year YEAR and then replace
                    // YEAR back by the real year and the unique replacement
                    // string back with YEAR. Notice that "all occurences of YEAR"
                    // means all occurences of 4 digit as well as 2 digit form!
                    //
                    // the bugs: we assume that neither of %c nor %x contains any
                    // fields which may change between the YEAR and real year. For
                    // example, the week number (%U, %W) and the day number (%j)
                    // will change if one of these years is leap and the other one
                    // is not!
                    {
                        // find the YEAR: normally, for any year X, Jan 1 or the
                        // year X + 28 is the same weekday as Jan 1 of X (because
                        // the weekday advances by 1 for each normal X and by 2
                        // for each leap X, hence by 5 every 4 years or by 35
                        // which is 0 mod 7 every 28 years) but this rule breaks
                        // down if there are years between X and Y which are
                        // divisible by 4 but not leap (i.e. divisible by 100 but
                        // not 400), hence the correction.

                        int yearReal = GetYear(tz);
                        int mod28 = yearReal % 28;

                        // be careful to not go too far - we risk to leave the
                        // supported range
                        int year;
                        if ( mod28 < 10 )
                        {
                            year = 1988 + mod28;      // 1988 == 0 (mod 28)
                        }
                        else
                        {
                            year = 1970 + mod28 - 10; // 1970 == 10 (mod 28)
                        }

                        int nCentury = year / 100,
                            nCenturyReal = yearReal / 100;

                        // need to adjust for the years divisble by 400 which are
                        // not leap but are counted like leap ones if we just take
                        // the number of centuries in between for nLostWeekDays
                        int nLostWeekDays = (nCentury - nCenturyReal) -
                                            (nCentury / 4 - nCenturyReal / 4);

                        // we have to gain back the "lost" weekdays: note that the
                        // effect of this loop is to not do anything to
                        // nLostWeekDays (which we won't use any more), but to
                        // (indirectly) set the year correctly
                        while ( (nLostWeekDays % 7) != 0 )
                        {
                            nLostWeekDays += year++ % 4 ? 1 : 2;
                        }

                        // at any rate, we couldn't go further than 1988 + 9 + 28!
                        wxASSERT_MSG( year < 2030,
                                      _T("logic error in wxDateTime::Format") );

                        wxString strYear, strYear2;
                        strYear.Printf(_T("%d"), year);
                        strYear2.Printf(_T("%d"), year % 100);

                        // find two strings not occuring in format (this is surely
                        // not optimal way of doing it... improvements welcome!)
                        wxString fmt = format;
                        wxString replacement = (wxChar)-1;
                        while ( fmt.Find(replacement) != wxNOT_FOUND )
                        {
                            replacement << (wxChar)-1;
                        }

                        wxString replacement2 = (wxChar)-2;
                        while ( fmt.Find(replacement) != wxNOT_FOUND )
                        {
                            replacement << (wxChar)-2;
                        }

                        // replace all occurences of year with it
                        bool wasReplaced = fmt.Replace(strYear, replacement) > 0;
                        if ( !wasReplaced )
                            wasReplaced = fmt.Replace(strYear2, replacement2) > 0;

                        // use strftime() to format the same date but in supported
                        // year
                        //
                        // NB: we assume that strftime() doesn't check for the
                        //     date validity and will happily format the date
                        //     corresponding to Feb 29 of a non leap year (which
                        //     may happen if yearReal was leap and year is not)
                        struct tm tmAdjusted;
                        InitTm(tmAdjusted);
                        tmAdjusted.tm_hour = tm.hour;
                        tmAdjusted.tm_min = tm.min;
                        tmAdjusted.tm_sec = tm.sec;
                        tmAdjusted.tm_wday = tm.GetWeekDay();
                        tmAdjusted.tm_yday = GetDayOfYear();
                        tmAdjusted.tm_mday = tm.mday;
                        tmAdjusted.tm_mon = tm.mon;
                        tmAdjusted.tm_year = year - 1900;
                        tmAdjusted.tm_isdst = 0; // no DST, already adjusted
                        wxString str = CallStrftime(*p == _T('c') ? _T("%c")
                                                                  : _T("%x"),
                                                    &tmAdjusted);

                        // now replace the occurence of 1999 with the real year
                        wxString strYearReal, strYearReal2;
                        strYearReal.Printf(_T("%04d"), yearReal);
                        strYearReal2.Printf(_T("%02d"), yearReal % 100);
                        str.Replace(strYear, strYearReal);
                        str.Replace(strYear2, strYearReal2);

                        // and replace back all occurences of replacement string
                        if ( wasReplaced )
                        {
                            str.Replace(replacement2, strYear2);
                            str.Replace(replacement, strYear);
                        }

                        res += str;
                    }
                    break;

                case _T('d'):       // day of a month (01-31)
                    res += wxString::Format(fmt, tm.mday);
                    break;

                case _T('H'):       // hour in 24h format (00-23)
                    res += wxString::Format(fmt, tm.hour);
                    break;

                case _T('I'):       // hour in 12h format (01-12)
                    {
                        // 24h -> 12h, 0h -> 12h too
                        int hour12 = tm.hour > 12 ? tm.hour - 12
                                                  : tm.hour ? tm.hour : 12;
                        res += wxString::Format(fmt, hour12);
                    }
                    break;

                case _T('j'):       // day of the year
                    res += wxString::Format(fmt, GetDayOfYear(tz));
                    break;

                case _T('l'):       // milliseconds (NOT STANDARD)
                    res += wxString::Format(fmt, GetMillisecond(tz));
                    break;

                case _T('m'):       // month as a number (01-12)
                    res += wxString::Format(fmt, tm.mon + 1);
                    break;

                case _T('M'):       // minute as a decimal number (00-59)
                    res += wxString::Format(fmt, tm.min);
                    break;

                case _T('p'):       // AM or PM string
                    res += CallStrftime(_T("%p"), &tmTimeOnly);
                    break;

                case _T('S'):       // second as a decimal number (00-61)
                    res += wxString::Format(fmt, tm.sec);
                    break;

                case _T('U'):       // week number in the year (Sunday 1st week day)
                    res += wxString::Format(fmt, GetWeekOfYear(Sunday_First, tz));
                    break;

                case _T('W'):       // week number in the year (Monday 1st week day)
                    res += wxString::Format(fmt, GetWeekOfYear(Monday_First, tz));
                    break;

                case _T('w'):       // weekday as a number (0-6), Sunday = 0
                    res += wxString::Format(fmt, tm.GetWeekDay());
                    break;

                // case _T('x'): -- handled with "%c"

                case _T('X'):       // locale default time representation
                    // just use strftime() to format the time for us
                    res += CallStrftime(_T("%X"), &tmTimeOnly);
                    break;

                case _T('y'):       // year without century (00-99)
                    res += wxString::Format(fmt, tm.year % 100);
                    break;

                case _T('Y'):       // year with century
                    res += wxString::Format(fmt, tm.year);
                    break;

                case _T('Z'):       // timezone name
                    res += CallStrftime(_T("%Z"), &tmTimeOnly);
                    break;

                default:
                    // is it the format width?
                    fmt.Empty();
                    while ( *p == _T('-') || *p == _T('+') ||
                            *p == _T(' ') || wxIsdigit(*p) )
                    {
                        fmt += *p;
                    }

                    if ( !fmt.IsEmpty() )
                    {
                        // we've only got the flags and width so far in fmt
                        fmt.Prepend(_T('%'));
                        fmt.Append(_T('d'));

                        restart = TRUE;

                        break;
                    }

                    // no, it wasn't the width
                    wxFAIL_MSG(_T("unknown format specificator"));

                    // fall through and just copy it nevertheless

                case _T('%'):       // a percent sign
                    res += *p;
                    break;

                case 0:             // the end of string
                    wxFAIL_MSG(_T("missing format at the end of string"));

                    // just put the '%' which was the last char in format
                    res += _T('%');
                    break;
            }
        }
    }

    return res;
}

// this function parses a string in (strict) RFC 822 format: see the section 5
// of the RFC for the detailed description, but briefly it's something of the
// form "Sat, 18 Dec 1999 00:48:30 +0100"
//
// this function is "strict" by design - it must reject anything except true
// RFC822 time specs.
//
// TODO a great candidate for using reg exps
const wxChar *wxDateTime::ParseRfc822Date(const wxChar* date)
{
    wxCHECK_MSG( date, (wxChar *)NULL, _T("NULL pointer in wxDateTime::Parse") );

    const wxChar *p = date;
    const wxChar *comma = wxStrchr(p, _T(','));
    if ( comma )
    {
        // the part before comma is the weekday

        // skip it for now - we don't use but might check that it really
        // corresponds to the specfied date
        p = comma + 1;

        if ( *p != _T(' ') )
        {
            wxLogDebug(_T("no space after weekday in RFC822 time spec"));

            return (wxChar *)NULL;
        }

        p++; // skip space
    }

    // the following 1 or 2 digits are the day number
    if ( !wxIsdigit(*p) )
    {
        wxLogDebug(_T("day number expected in RFC822 time spec, none found"));

        return (wxChar *)NULL;
    }

    wxDateTime_t day = *p++ - _T('0');
    if ( wxIsdigit(*p) )
    {
        day *= 10;
        day += *p++ - _T('0');
    }

    if ( *p++ != _T(' ') )
    {
        return (wxChar *)NULL;
    }

    // the following 3 letters specify the month
    wxString monName(p, 3);
    Month mon;
    if ( monName == _T("Jan") )
        mon = Jan;
    else if ( monName == _T("Feb") )
        mon = Feb;
    else if ( monName == _T("Mar") )
        mon = Mar;
    else if ( monName == _T("Apr") )
        mon = Apr;
    else if ( monName == _T("May") )
        mon = May;
    else if ( monName == _T("Jun") )
        mon = Jun;
    else if ( monName == _T("Jul") )
        mon = Jul;
    else if ( monName == _T("Aug") )
        mon = Aug;
    else if ( monName == _T("Sep") )
        mon = Sep;
    else if ( monName == _T("Oct") )
        mon = Oct;
    else if ( monName == _T("Nov") )
        mon = Nov;
    else if ( monName == _T("Dec") )
        mon = Dec;
    else
    {
        wxLogDebug(_T("Invalid RFC 822 month name '%s'"), monName.c_str());

        return (wxChar *)NULL;
    }

    p += 3;

    if ( *p++ != _T(' ') )
    {
        return (wxChar *)NULL;
    }

    // next is the year
    if ( !wxIsdigit(*p) )
    {
        // no year?
        return (wxChar *)NULL;
    }

    int year = *p++ - _T('0');

    if ( !wxIsdigit(*p) )
    {
        // should have at least 2 digits in the year
        return (wxChar *)NULL;
    }

    year *= 10;
    year += *p++ - _T('0');

    // is it a 2 digit year (as per original RFC 822) or a 4 digit one?
    if ( wxIsdigit(*p) )
    {
        year *= 10;
        year += *p++ - _T('0');

        if ( !wxIsdigit(*p) )
        {
            // no 3 digit years please
            return (wxChar *)NULL;
        }

        year *= 10;
        year += *p++ - _T('0');
    }

    if ( *p++ != _T(' ') )
    {
        return (wxChar *)NULL;
    }

    // time is in the format hh:mm:ss and seconds are optional
    if ( !wxIsdigit(*p) )
    {
        return (wxChar *)NULL;
    }

    wxDateTime_t hour = *p++ - _T('0');

    if ( !wxIsdigit(*p) )
    {
        return (wxChar *)NULL;
    }

    hour *= 10;
    hour += *p++ - _T('0');

    if ( *p++ != _T(':') )
    {
        return (wxChar *)NULL;
    }

    if ( !wxIsdigit(*p) )
    {
        return (wxChar *)NULL;
    }

    wxDateTime_t min = *p++ - _T('0');

    if ( !wxIsdigit(*p) )
    {
        return (wxChar *)NULL;
    }

    min *= 10;
    min += *p++ - _T('0');

    wxDateTime_t sec = 0;
    if ( *p++ == _T(':') )
    {
        if ( !wxIsdigit(*p) )
        {
            return (wxChar *)NULL;
        }

        sec = *p++ - _T('0');

        if ( !wxIsdigit(*p) )
        {
            return (wxChar *)NULL;
        }

        sec *= 10;
        sec += *p++ - _T('0');
    }

    if ( *p++ != _T(' ') )
    {
        return (wxChar *)NULL;
    }

    // and now the interesting part: the timezone
    int offset;
    if ( *p == _T('-') || *p == _T('+') )
    {
        // the explicit offset given: it has the form of hhmm
        bool plus = *p++ == _T('+');

        if ( !wxIsdigit(*p) || !wxIsdigit(*(p + 1)) )
        {
            return (wxChar *)NULL;
        }

        // hours
        offset = 60*(10*(*p - _T('0')) + (*(p + 1) - _T('0')));

        p += 2;

        if ( !wxIsdigit(*p) || !wxIsdigit(*(p + 1)) )
        {
            return (wxChar *)NULL;
        }

        // minutes
        offset += 10*(*p - _T('0')) + (*(p + 1) - _T('0'));

        if ( !plus )
        {
            offset = -offset;
        }

        p += 2;
    }
    else
    {
        // the symbolic timezone given: may be either military timezone or one
        // of standard abbreviations
        if ( !*(p + 1) )
        {
            // military: Z = UTC, J unused, A = -1, ..., Y = +12
            static const int offsets[26] =
            {
                //A  B   C   D   E   F   G   H   I    J    K    L    M
                -1, -2, -3, -4, -5, -6, -7, -8, -9,   0, -10, -11, -12,
                //N  O   P   R   Q   S   T   U   V    W    Z    Y    Z
                +1, +2, +3, +4, +5, +6, +7, +8, +9, +10, +11, +12, 0
            };

            if ( *p < _T('A') || *p > _T('Z') || *p == _T('J') )
            {
                wxLogDebug(_T("Invalid militaty timezone '%c'"), *p);

                return (wxChar *)NULL;
            }

            offset = offsets[*p++ - _T('A')];
        }
        else
        {
            // abbreviation
            wxString tz = p;
            if ( tz == _T("UT") || tz == _T("UTC") || tz == _T("GMT") )
                offset = 0;
            else if ( tz == _T("AST") )
                offset = AST - GMT0;
            else if ( tz == _T("ADT") )
                offset = ADT - GMT0;
            else if ( tz == _T("EST") )
                offset = EST - GMT0;
            else if ( tz == _T("EDT") )
                offset = EDT - GMT0;
            else if ( tz == _T("CST") )
                offset = CST - GMT0;
            else if ( tz == _T("CDT") )
                offset = CDT - GMT0;
            else if ( tz == _T("MST") )
                offset = MST - GMT0;
            else if ( tz == _T("MDT") )
                offset = MDT - GMT0;
            else if ( tz == _T("PST") )
                offset = PST - GMT0;
            else if ( tz == _T("PDT") )
                offset = PDT - GMT0;
            else
            {
                wxLogDebug(_T("Unknown RFC 822 timezone '%s'"), p);

                return (wxChar *)NULL;
            }

            p += tz.length();
        }

        // make it minutes
        offset *= 60;
    }

    // the spec was correct
    Set(day, mon, year, hour, min, sec);
    MakeTimezone((wxDateTime_t)(60*offset));

    return p;
}

const wxChar *wxDateTime::ParseFormat(const wxChar *date,
                                      const wxChar *format,
                                      const wxDateTime& dateDef)
{
    wxCHECK_MSG( date && format, (wxChar *)NULL,
                 _T("NULL pointer in wxDateTime::ParseFormat()") );

    wxString str;
    unsigned long num;

    // what fields have we found?
    bool haveWDay = FALSE,
         haveYDay = FALSE,
         haveDay = FALSE,
         haveMon = FALSE,
         haveYear = FALSE,
         haveHour = FALSE,
         haveMin = FALSE,
         haveSec = FALSE;

    bool hourIsIn12hFormat = FALSE, // or in 24h one?
         isPM = FALSE;              // AM by default

    // and the value of the items we have (init them to get rid of warnings)
    wxDateTime_t sec = 0,
                 min = 0,
                 hour = 0;
    WeekDay wday = Inv_WeekDay;
    wxDateTime_t yday = 0,
                 mday = 0;
    wxDateTime::Month mon = Inv_Month;
    int year = 0;

    const wxChar *input = date;
    for ( const wxChar *fmt = format; *fmt; fmt++ )
    {
        if ( *fmt != _T('%') )
        {
            if ( wxIsspace(*fmt) )
            {
                // a white space in the format string matches 0 or more white
                // spaces in the input
                while ( wxIsspace(*input) )
                {
                    input++;
                }
            }
            else // !space
            {
                // any other character (not whitespace, not '%') must be
                // matched by itself in the input
                if ( *input++ != *fmt )
                {
                    // no match
                    return (wxChar *)NULL;
                }
            }

            // done with this format char
            continue;
        }

        // start of a format specification

        // parse the optional width
        size_t width = 0;
        while ( isdigit(*++fmt) )
        {
            width *= 10;
            width += *fmt - _T('0');
        }

        // the default widths for the various fields
        if ( !width )
        {
            switch ( *fmt )
            {
                case _T('Y'):               // year has 4 digits
                    width = 4;
                    break;

                case _T('j'):               // day of year has 3 digits
                case _T('l'):               // milliseconds have 3 digits
                    width = 3;
                    break;

                case _T('w'):               // week day as number has only one
                    width = 1;
                    break;

                default:
                    // default for all other fields
                    width = 2;
            }
        }

        // then the format itself
        switch ( *fmt )
        {
            case _T('a'):       // a weekday name
            case _T('A'):
                {
                    int flag = *fmt == _T('a') ? Name_Abbr : Name_Full;
                    wday = GetWeekDayFromName(GetAlphaToken(input), flag);
                    if ( wday == Inv_WeekDay )
                    {
                        // no match
                        return (wxChar *)NULL;
                    }
                }
                haveWDay = TRUE;
                break;

            case _T('b'):       // a month name
            case _T('B'):
                {
                    int flag = *fmt == _T('b') ? Name_Abbr : Name_Full;
                    mon = GetMonthFromName(GetAlphaToken(input), flag);
                    if ( mon == Inv_Month )
                    {
                        // no match
                        return (wxChar *)NULL;
                    }
                }
                haveMon = TRUE;
                break;

            case _T('c'):       // locale default date and time  representation
                {
                    wxDateTime dt;

                    // this is the format which corresponds to ctime() output
                    // and strptime("%c") should parse it, so try it first
                    static const wxChar *fmtCtime = _T("%a %b %d %H:%M:%S %Y");

                    const wxChar *result = dt.ParseFormat(input, fmtCtime);
                    if ( !result )
                    {
                        result = dt.ParseFormat(input, _T("%x %X"));
                    }

                    if ( !result )
                    {
                        result = dt.ParseFormat(input, _T("%X %x"));
                    }

                    if ( !result )
                    {
                        // we've tried everything and still no match
                        return (wxChar *)NULL;
                    }

                    Tm tm = dt.GetTm();

                    haveDay = haveMon = haveYear =
                    haveHour = haveMin = haveSec = TRUE;

                    hour = tm.hour;
                    min = tm.min;
                    sec = tm.sec;

                    year = tm.year;
                    mon = tm.mon;
                    mday = tm.mday;

                    input = result;
                }
                break;

            case _T('d'):       // day of a month (01-31)
                if ( !GetNumericToken(width, input, &num) ||
                        (num > 31) || (num < 1) )
                {
                    // no match
                    return (wxChar *)NULL;
                }

                // we can't check whether the day range is correct yet, will
                // do it later - assume ok for now
                haveDay = TRUE;
                mday = (wxDateTime_t)num;
                break;

            case _T('H'):       // hour in 24h format (00-23)
                if ( !GetNumericToken(width, input, &num) || (num > 23) )
                {
                    // no match
                    return (wxChar *)NULL;
                }

                haveHour = TRUE;
                hour = (wxDateTime_t)num;
                break;

            case _T('I'):       // hour in 12h format (01-12)
                if ( !GetNumericToken(width, input, &num) || !num || (num > 12) )
                {
                    // no match
                    return (wxChar *)NULL;
                }

                haveHour = TRUE;
                hourIsIn12hFormat = TRUE;
                hour = (wxDateTime_t)(num % 12);        // 12 should be 0
                break;

            case _T('j'):       // day of the year
                if ( !GetNumericToken(width, input, &num) || !num || (num > 366) )
                {
                    // no match
                    return (wxChar *)NULL;
                }

                haveYDay = TRUE;
                yday = (wxDateTime_t)num;
                break;

            case _T('m'):       // month as a number (01-12)
                if ( !GetNumericToken(width, input, &num) || !num || (num > 12) )
                {
                    // no match
                    return (wxChar *)NULL;
                }

                haveMon = TRUE;
                mon = (Month)(num - 1);
                break;

            case _T('M'):       // minute as a decimal number (00-59)
                if ( !GetNumericToken(width, input, &num) || (num > 59) )
                {
                    // no match
                    return (wxChar *)NULL;
                }

                haveMin = TRUE;
                min = (wxDateTime_t)num;
                break;

            case _T('p'):       // AM or PM string
                {
                    wxString am, pm, token = GetAlphaToken(input);

                    GetAmPmStrings(&am, &pm);
                    if ( token.CmpNoCase(pm) == 0 )
                    {
                        isPM = TRUE;
                    }
                    else if ( token.CmpNoCase(am) != 0 )
                    {
                        // no match
                        return (wxChar *)NULL;
                    }
                }
                break;

            case _T('r'):       // time as %I:%M:%S %p
                {
                    wxDateTime dt;
                    input = dt.ParseFormat(input, _T("%I:%M:%S %p"));
                    if ( !input )
                    {
                        // no match
                        return (wxChar *)NULL;
                    }

                    haveHour = haveMin = haveSec = TRUE;

                    Tm tm = dt.GetTm();
                    hour = tm.hour;
                    min = tm.min;
                    sec = tm.sec;
                }
                break;

            case _T('R'):       // time as %H:%M
                {
                    wxDateTime dt;
                    input = dt.ParseFormat(input, _T("%H:%M"));
                    if ( !input )
                    {
                        // no match
                        return (wxChar *)NULL;
                    }

                    haveHour = haveMin = TRUE;

                    Tm tm = dt.GetTm();
                    hour = tm.hour;
                    min = tm.min;
                }

            case _T('S'):       // second as a decimal number (00-61)
                if ( !GetNumericToken(width, input, &num) || (num > 61) )
                {
                    // no match
                    return (wxChar *)NULL;
                }

                haveSec = TRUE;
                sec = (wxDateTime_t)num;
                break;

            case _T('T'):       // time as %H:%M:%S
                {
                    wxDateTime dt;
                    input = dt.ParseFormat(input, _T("%H:%M:%S"));
                    if ( !input )
                    {
                        // no match
                        return (wxChar *)NULL;
                    }

                    haveHour = haveMin = haveSec = TRUE;

                    Tm tm = dt.GetTm();
                    hour = tm.hour;
                    min = tm.min;
                    sec = tm.sec;
                }
                break;

            case _T('w'):       // weekday as a number (0-6), Sunday = 0
                if ( !GetNumericToken(width, input, &num) || (wday > 6) )
                {
                    // no match
                    return (wxChar *)NULL;
                }

                haveWDay = TRUE;
                wday = (WeekDay)num;
                break;

            case _T('x'):       // locale default date representation
#ifdef HAVE_STRPTIME
                // try using strptime() - it may fail even if the input is
                // correct but the date is out of range, so we will fall back
                // to our generic code anyhow
                {
                    struct tm tm;
                    const wxChar *result = CallStrptime(input, "%x", &tm);
                    if ( result )
                    {
                        input = result;

                        haveDay = haveMon = haveYear = TRUE;

                        year = 1900 + tm.tm_year;
                        mon = (Month)tm.tm_mon;
                        mday = tm.tm_mday;

                        break;
                    }
                }
#endif // HAVE_STRPTIME

                // TODO query the LOCALE_IDATE setting under Win32
                {
                    wxDateTime dt;

                    wxString fmtDate, fmtDateAlt;
                    if ( IsWestEuropeanCountry(GetCountry()) ||
                         GetCountry() == Russia )
                    {
                        fmtDate = _T("%d/%m/%y");
                        fmtDateAlt = _T("%m/%d/%y");
                    }
                    else // assume USA
                    {
                        fmtDate = _T("%m/%d/%y");
                        fmtDateAlt = _T("%d/%m/%y");
                    }

                    const wxChar *result = dt.ParseFormat(input, fmtDate);

                    if ( !result )
                    {
                        // ok, be nice and try another one
                        result = dt.ParseFormat(input, fmtDateAlt);
                    }

                    if ( !result )
                    {
                        // bad luck
                        return (wxChar *)NULL;
                    }

                    Tm tm = dt.GetTm();

                    haveDay = haveMon = haveYear = TRUE;

                    year = tm.year;
                    mon = tm.mon;
                    mday = tm.mday;

                    input = result;
                }

                break;

            case _T('X'):       // locale default time representation
#ifdef HAVE_STRPTIME
                {
                    // use strptime() to do it for us
                    struct tm tm;
                    input = CallStrptime(input, "%X", &tm);
                    if ( !input )
                    {
                        return (wxChar *)NULL;
                    }

                    haveHour = haveMin = haveSec = TRUE;

                    hour = tm.tm_hour;
                    min = tm.tm_min;
                    sec = tm.tm_sec;
                }
#else // !HAVE_STRPTIME
                // TODO under Win32 we can query the LOCALE_ITIME system
                //      setting which says whether the default time format is
                //      24 or 12 hour
                {
                    // try to parse what follows as "%H:%M:%S" and, if this
                    // fails, as "%I:%M:%S %p" - this should catch the most
                    // common cases
                    wxDateTime dt;

                    const wxChar *result = dt.ParseFormat(input, _T("%T"));
                    if ( !result )
                    {
                        result = dt.ParseFormat(input, _T("%r"));
                    }

                    if ( !result )
                    {
                        // no match
                        return (wxChar *)NULL;
                    }

                    haveHour = haveMin = haveSec = TRUE;

                    Tm tm = dt.GetTm();
                    hour = tm.hour;
                    min = tm.min;
                    sec = tm.sec;

                    input = result;
                }
#endif // HAVE_STRPTIME/!HAVE_STRPTIME
                break;

            case _T('y'):       // year without century (00-99)
                if ( !GetNumericToken(width, input, &num) || (num > 99) )
                {
                    // no match
                    return (wxChar *)NULL;
                }

                haveYear = TRUE;

                // TODO should have an option for roll over date instead of
                //      hard coding it here
                year = (num > 30 ? 1900 : 2000) + (wxDateTime_t)num;
                break;

            case _T('Y'):       // year with century
                if ( !GetNumericToken(width, input, &num) )
                {
                    // no match
                    return (wxChar *)NULL;
                }

                haveYear = TRUE;
                year = (wxDateTime_t)num;
                break;

            case _T('Z'):       // timezone name
                wxFAIL_MSG(_T("TODO"));
                break;

            case _T('%'):       // a percent sign
                if ( *input++ != _T('%') )
                {
                    // no match
                    return (wxChar *)NULL;
                }
                break;

            case 0:             // the end of string
                wxFAIL_MSG(_T("unexpected format end"));

                // fall through

            default:            // not a known format spec
                return (wxChar *)NULL;
        }
    }

    // format matched, try to construct a date from what we have now
    Tm tmDef;
    if ( dateDef.IsValid() )
    {
        // take this date as default
        tmDef = dateDef.GetTm();
    }
    else if ( IsValid() )
    {
        // if this date is valid, don't change it
        tmDef = GetTm();
    }
    else
    {
        // no default and this date is invalid - fall back to Today()
        tmDef = Today().GetTm();
    }

    Tm tm = tmDef;

    // set the date
    if ( haveYear )
    {
        tm.year = year;
    }

    // TODO we don't check here that the values are consistent, if both year
    //      day and month/day were found, we just ignore the year day and we
    //      also always ignore the week day
    if ( haveMon && haveDay )
    {
        if ( mday > GetNumOfDaysInMonth(tm.year, mon) )
        {
            wxLogDebug(_T("bad month day in wxDateTime::ParseFormat"));

            return (wxChar *)NULL;
        }

        tm.mon = mon;
        tm.mday = mday;
    }
    else if ( haveYDay )
    {
        if ( yday > GetNumberOfDays(tm.year) )
        {
            wxLogDebug(_T("bad year day in wxDateTime::ParseFormat"));

            return (wxChar *)NULL;
        }

        Tm tm2 = wxDateTime(1, Jan, tm.year).SetToYearDay(yday).GetTm();

        tm.mon = tm2.mon;
        tm.mday = tm2.mday;
    }

    // deal with AM/PM
    if ( haveHour && hourIsIn12hFormat && isPM )
    {
        // translate to 24hour format
        hour += 12;
    }
    //else: either already in 24h format or no translation needed

    // set the time
    if ( haveHour )
    {
        tm.hour = hour;
    }

    if ( haveMin )
    {
        tm.min = min;
    }

    if ( haveSec )
    {
        tm.sec = sec;
    }

    Set(tm);

    return input;
}

const wxChar *wxDateTime::ParseDateTime(const wxChar *date)
{
    wxCHECK_MSG( date, (wxChar *)NULL, _T("NULL pointer in wxDateTime::Parse") );

    // there is a public domain version of getdate.y, but it only works for
    // English...
    wxFAIL_MSG(_T("TODO"));

    return (wxChar *)NULL;
}

const wxChar *wxDateTime::ParseDate(const wxChar *date)
{
    // this is a simplified version of ParseDateTime() which understands only
    // "today" (for wxDate compatibility) and digits only otherwise (and not
    // all esoteric constructions ParseDateTime() knows about)

    wxCHECK_MSG( date, (wxChar *)NULL, _T("NULL pointer in wxDateTime::Parse") );

    const wxChar *p = date;
    while ( wxIsspace(*p) )
        p++;

    // some special cases
    static struct
    {
        const wxChar *str;
        int dayDiffFromToday;
    } literalDates[] =
    {
        { wxTRANSLATE("today"),             0 },
        { wxTRANSLATE("yesterday"),        -1 },
        { wxTRANSLATE("tomorrow"),          1 },
    };

    for ( size_t n = 0; n < WXSIZEOF(literalDates); n++ )
    {
        wxString date = wxGetTranslation(literalDates[n].str);
        size_t len = date.length();
        if ( wxStrlen(p) >= len && (wxString(p, len).CmpNoCase(date) == 0) )
        {
            // nothing can follow this, so stop here
            p += len;

            int dayDiffFromToday = literalDates[n].dayDiffFromToday;
            *this = Today();
            if ( dayDiffFromToday )
            {
                *this += wxDateSpan::Days(dayDiffFromToday);
            }

            return p;
        }
    }

    // We try to guess what we have here: for each new (numeric) token, we
    // determine if it can be a month, day or a year. Of course, there is an
    // ambiguity as some numbers may be days as well as months, so we also
    // have the ability to back track.

    // what do we have?
    bool haveDay = FALSE,       // the months day?
         haveWDay = FALSE,      // the day of week?
         haveMon = FALSE,       // the month?
         haveYear = FALSE;      // the year?

    // and the value of the items we have (init them to get rid of warnings)
    WeekDay wday = Inv_WeekDay;
    wxDateTime_t day = 0;
    wxDateTime::Month mon = Inv_Month;
    int year = 0;

    // tokenize the string
    size_t nPosCur = 0;
    static const wxChar *dateDelimiters = _T(".,/-\t\r\n ");
    wxStringTokenizer tok(p, dateDelimiters);
    while ( tok.HasMoreTokens() )
    {
        wxString token = tok.GetNextToken();
        if ( !token )
            continue;

        // is it a number?
        unsigned long val;
        if ( token.ToULong(&val) )
        {
            // guess what this number is

            bool isDay = FALSE,
                 isMonth = FALSE,
                 isYear = FALSE;

            if ( !haveMon && val > 0 && val <= 12 )
            {
                // assume it is month
                isMonth = TRUE;
            }
            else // not the month
            {
                wxDateTime_t maxDays = haveMon
                    ? GetNumOfDaysInMonth(haveYear ? year : Inv_Year, mon)
                    : 31;

                // can it be day?
                if ( (val == 0) || (val > (unsigned long)maxDays) )  // cast to shut up compiler warning in BCC
                {
                    isYear = TRUE;
                }
                else
                {
                    isDay = TRUE;
                }
            }

            if ( isYear )
            {
                if ( haveYear )
                    break;

                haveYear = TRUE;

                year = (wxDateTime_t)val;
            }
            else if ( isDay )
            {
                if ( haveDay )
                    break;

                haveDay = TRUE;

                day = (wxDateTime_t)val;
            }
            else if ( isMonth )
            {
                haveMon = TRUE;

                mon = (Month)(val - 1);
            }
        }
        else // not a number
        {
            // be careful not to overwrite the current mon value
            Month mon2 = GetMonthFromName(token, Name_Full | Name_Abbr);
            if ( mon2 != Inv_Month )
            {
                // it's a month
                if ( haveMon )
                {
                    // but we already have a month - maybe we guessed wrong?
                    if ( !haveDay )
                    {
                        // no need to check in month range as always < 12, but
                        // the days are counted from 1 unlike the months
                        day = (wxDateTime_t)mon + 1;
                        haveDay = TRUE;
                    }
                    else
                    {
                        // could possible be the year (doesn't the year come
                        // before the month in the japanese format?) (FIXME)
                        break;
                    }
                }

                mon = mon2;

                haveMon = TRUE;
            }
            else // not a valid month name
            {
                wday = GetWeekDayFromName(token, Name_Full | Name_Abbr);
                if ( wday != Inv_WeekDay )
                {
                    // a week day
                    if ( haveWDay )
                    {
                        break;
                    }

                    haveWDay = TRUE;
                }
                else // not a valid weekday name
                {
                    // try the ordinals
                    static const wxChar *ordinals[] =
                    {
                        wxTRANSLATE("first"),
                        wxTRANSLATE("second"),
                        wxTRANSLATE("third"),
                        wxTRANSLATE("fourth"),
                        wxTRANSLATE("fifth"),
                        wxTRANSLATE("sixth"),
                        wxTRANSLATE("seventh"),
                        wxTRANSLATE("eighth"),
                        wxTRANSLATE("ninth"),
                        wxTRANSLATE("tenth"),
                        wxTRANSLATE("eleventh"),
                        wxTRANSLATE("twelfth"),
                        wxTRANSLATE("thirteenth"),
                        wxTRANSLATE("fourteenth"),
                        wxTRANSLATE("fifteenth"),
                        wxTRANSLATE("sixteenth"),
                        wxTRANSLATE("seventeenth"),
                        wxTRANSLATE("eighteenth"),
                        wxTRANSLATE("nineteenth"),
                        wxTRANSLATE("twentieth"),
                        // that's enough - otherwise we'd have problems with
                        // composite (or not) ordinals
                    };

                    size_t n;
                    for ( n = 0; n < WXSIZEOF(ordinals); n++ )
                    {
                        if ( token.CmpNoCase(ordinals[n]) == 0 )
                        {
                            break;
                        }
                    }

                    if ( n == WXSIZEOF(ordinals) )
                    {
                        // stop here - something unknown
                        break;
                    }

                    // it's a day
                    if ( haveDay )
                    {
                        // don't try anything here (as in case of numeric day
                        // above) - the symbolic day spec should always
                        // precede the month/year
                        break;
                    }

                    haveDay = TRUE;

                    day = (wxDateTime_t)(n + 1);
                }
            }
        }

        nPosCur = tok.GetPosition();
    }

    // either no more tokens or the scan was stopped by something we couldn't
    // parse - in any case, see if we can construct a date from what we have
    if ( !haveDay && !haveWDay )
    {
        wxLogDebug(_T("ParseDate: no day, no weekday hence no date."));

        return (wxChar *)NULL;
    }

    if ( haveWDay && (haveMon || haveYear || haveDay) &&
         !(haveDay && haveMon && haveYear) )
    {
        // without adjectives (which we don't support here) the week day only
        // makes sense completely separately or with the full date
        // specification (what would "Wed 1999" mean?)
        return (wxChar *)NULL;
    }

    if ( !haveWDay && haveYear && !(haveDay && haveMon) )
    {
        // may be we have month and day instead of day and year?
        if ( haveDay && !haveMon )
        {
            if ( day <= 12  )
            {
                // exchange day and month
                mon = (wxDateTime::Month)(day - 1);

                // we're in the current year then
                if ( (year > 0) &&
                        (unsigned)year <= GetNumOfDaysInMonth(Inv_Year, mon) )
                {
                    day = year;

                    haveMon = TRUE;
                    haveYear = FALSE;
                }
                //else: no, can't exchange, leave haveMon == FALSE
            }
        }

        if ( !haveMon )
        {
            // if we give the year, month and day must be given too
            wxLogDebug(_T("ParseDate: day and month should be specified if year is."));

            return (wxChar *)NULL;
        }
    }

    if ( !haveMon )
    {
        mon = GetCurrentMonth();
    }

    if ( !haveYear )
    {
        year = GetCurrentYear();
    }

    if ( haveDay )
    {
        Set(day, mon, year);

        if ( haveWDay )
        {
            // check that it is really the same
            if ( GetWeekDay() != wday )
            {
                // inconsistency detected
                wxLogDebug(_T("ParseDate: inconsistent day/weekday."));

                return (wxChar *)NULL;
            }
        }
    }
    else // haveWDay
    {
        *this = Today();

        SetToWeekDayInSameWeek(wday);
    }

    // return the pointer to the first unparsed char
    p += nPosCur;
    if ( nPosCur && wxStrchr(dateDelimiters, *(p - 1)) )
    {
        // if we couldn't parse the token after the delimiter, put back the
        // delimiter as well
        p--;
    }

    return p;
}

const wxChar *wxDateTime::ParseTime(const wxChar *time)
{
    wxCHECK_MSG( time, (wxChar *)NULL, _T("NULL pointer in wxDateTime::Parse") );

    // first try some extra things
    static const struct
    {
        const wxChar *name;
        wxDateTime_t  hour;
    } stdTimes[] =
    {
        { wxTRANSLATE("noon"),      12 },
        { wxTRANSLATE("midnight"),  00 },
        // anything else?
    };

    for ( size_t n = 0; n < WXSIZEOF(stdTimes); n++ )
    {
        wxString timeString = wxGetTranslation(stdTimes[n].name);
        size_t len = timeString.length();
        if ( timeString.CmpNoCase(wxString(time, len)) == 0 )
        {
            Set(stdTimes[n].hour, 0, 0);

            return time + len;
        }
    }

    // try all time formats we may think about in the order from longest to
    // shortest

    // 12hour with AM/PM?
    const wxChar *result = ParseFormat(time, _T("%I:%M:%S %p"));

    if ( !result )
    {
        // normally, it's the same, but why not try it?
        result = ParseFormat(time, _T("%H:%M:%S"));
    }

    if ( !result )
    {
        // 12hour with AM/PM but without seconds?
        result = ParseFormat(time, _T("%I:%M %p"));
    }

    if ( !result )
    {
        // without seconds?
        result = ParseFormat(time, _T("%H:%M"));
    }

    if ( !result )
    {
        // just the hour and AM/PM?
        result = ParseFormat(time, _T("%I %p"));
    }

    if ( !result )
    {
        // just the hour?
        result = ParseFormat(time, _T("%H"));
    }

    if ( !result )
    {
        // parse the standard format: normally it is one of the formats above
        // but it may be set to something completely different by the user
        result = ParseFormat(time, _T("%X"));
    }

    // TODO: parse timezones

    return result;
}

// ----------------------------------------------------------------------------
// Workdays and holidays support
// ----------------------------------------------------------------------------

bool wxDateTime::IsWorkDay(Country WXUNUSED(country)) const
{
    return !wxDateTimeHolidayAuthority::IsHoliday(*this);
}

// ============================================================================
// wxTimeSpan
// ============================================================================

// this enum is only used in wxTimeSpan::Format() below but we can't declare
// it locally to the method as it provokes an internal compiler error in egcs
// 2.91.60 when building with -O2
enum TimeSpanPart
{
    Part_Week,
    Part_Day,
    Part_Hour,
    Part_Min,
    Part_Sec,
    Part_MSec
};

// not all strftime(3) format specifiers make sense here because, for example,
// a time span doesn't have a year nor a timezone
//
// Here are the ones which are supported (all of them are supported by strftime
// as well):
//  %H          hour in 24 hour format
//  %M          minute (00 - 59)
//  %S          second (00 - 59)
//  %%          percent sign
//
// Also, for MFC CTimeSpan compatibility, we support
//  %D          number of days
//
// And, to be better than MFC :-), we also have
//  %E          number of wEeks
//  %l          milliseconds (000 - 999)
wxString wxTimeSpan::Format(const wxChar *format) const
{
    wxCHECK_MSG( format, _T(""), _T("NULL format in wxTimeSpan::Format") );

    wxString str;
    str.Alloc(wxStrlen(format));

    // Suppose we have wxTimeSpan ts(1 /* hour */, 2 /* min */, 3 /* sec */)
    //
    // Then, of course, ts.Format("%H:%M:%S") must return "01:02:03", but the
    // question is what should ts.Format("%S") do? The code here returns "3273"
    // in this case (i.e. the total number of seconds, not just seconds % 60)
    // because, for me, this call means "give me entire time interval in
    // seconds" and not "give me the seconds part of the time interval"
    //
    // If we agree that it should behave like this, it is clear that the
    // interpretation of each format specifier depends on the presence of the
    // other format specs in the string: if there was "%H" before "%M", we
    // should use GetMinutes() % 60, otherwise just GetMinutes() &c

    // we remember the most important unit found so far
    TimeSpanPart partBiggest = Part_MSec;

    for ( const wxChar *pch = format; *pch; pch++ )
    {
        wxChar ch = *pch;

        if ( ch == _T('%') )
        {
            // the start of the format specification of the printf() below
            wxString fmtPrefix = _T('%');

            // the number
            long n;

            ch = *++pch;    // get the format spec char
            switch ( ch )
            {
                default:
                    wxFAIL_MSG( _T("invalid format character") );
                    // fall through

                case _T('%'):
                    str += ch;

                    // skip the part below switch
                    continue;

                case _T('D'):
                    n = GetDays();
                    if ( partBiggest < Part_Day )
                    {
                        n %= DAYS_PER_WEEK;
                    }
                    else
                    {
                        partBiggest = Part_Day;
                    }
                    break;

                case _T('E'):
                    partBiggest = Part_Week;
                    n = GetWeeks();
                    break;

                case _T('H'):
                    n = GetHours();
                    if ( partBiggest < Part_Hour )
                    {
                        n %= HOURS_PER_DAY;
                    }
                    else
                    {
                        partBiggest = Part_Hour;
                    }

                    fmtPrefix += _T("02");
                    break;

                case _T('l'):
                    n = GetMilliseconds().ToLong();
                    if ( partBiggest < Part_MSec )
                    {
                        n %= 1000;
                    }
                    //else: no need to reset partBiggest to Part_MSec, it is
                    //      the least significant one anyhow

                    fmtPrefix += _T("03");
                    break;

                case _T('M'):
                    n = GetMinutes();
                    if ( partBiggest < Part_Min )
                    {
                        n %= MIN_PER_HOUR;
                    }
                    else
                    {
                        partBiggest = Part_Min;
                    }

                    fmtPrefix += _T("02");
                    break;

                case _T('S'):
                    n = GetSeconds().ToLong();
                    if ( partBiggest < Part_Sec )
                    {
                        n %= SEC_PER_MIN;
                    }
                    else
                    {
                        partBiggest = Part_Sec;
                    }

                    fmtPrefix += _T("02");
                    break;
            }

            str += wxString::Format(fmtPrefix + _T("ld"), n);
        }
        else
        {
            // normal character, just copy
            str += ch;
        }
    }

    return str;
}

// ============================================================================
// wxDateTimeHolidayAuthority and related classes
// ============================================================================

#include "wx/arrimpl.cpp"

WX_DEFINE_OBJARRAY(wxDateTimeArray);

static int wxCMPFUNC_CONV
wxDateTimeCompareFunc(wxDateTime **first, wxDateTime **second)
{
    wxDateTime dt1 = **first,
               dt2 = **second;

    return dt1 == dt2 ? 0 : dt1 < dt2 ? -1 : +1;
}

// ----------------------------------------------------------------------------
// wxDateTimeHolidayAuthority
// ----------------------------------------------------------------------------

wxHolidayAuthoritiesArray wxDateTimeHolidayAuthority::ms_authorities;

/* static */
bool wxDateTimeHolidayAuthority::IsHoliday(const wxDateTime& dt)
{
    size_t count = ms_authorities.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        if ( ms_authorities[n]->DoIsHoliday(dt) )
        {
            return TRUE;
        }
    }

    return FALSE;
}

/* static */
size_t
wxDateTimeHolidayAuthority::GetHolidaysInRange(const wxDateTime& dtStart,
                                               const wxDateTime& dtEnd,
                                               wxDateTimeArray& holidays)
{
    wxDateTimeArray hol;

    holidays.Empty();

    size_t count = ms_authorities.GetCount();
    for ( size_t nAuth = 0; nAuth < count; nAuth++ )
    {
        ms_authorities[nAuth]->DoGetHolidaysInRange(dtStart, dtEnd, hol);

        WX_APPEND_ARRAY(holidays, hol);
    }

    holidays.Sort(wxDateTimeCompareFunc);

    return holidays.GetCount();
}

/* static */
void wxDateTimeHolidayAuthority::ClearAllAuthorities()
{
    WX_CLEAR_ARRAY(ms_authorities);
}

/* static */
void wxDateTimeHolidayAuthority::AddAuthority(wxDateTimeHolidayAuthority *auth)
{
    ms_authorities.Add(auth);
}

// ----------------------------------------------------------------------------
// wxDateTimeWorkDays
// ----------------------------------------------------------------------------

bool wxDateTimeWorkDays::DoIsHoliday(const wxDateTime& dt) const
{
    wxDateTime::WeekDay wd = dt.GetWeekDay();

    return (wd == wxDateTime::Sun) || (wd == wxDateTime::Sat);
}

size_t wxDateTimeWorkDays::DoGetHolidaysInRange(const wxDateTime& dtStart,
                                                const wxDateTime& dtEnd,
                                                wxDateTimeArray& holidays) const
{
    if ( dtStart > dtEnd )
    {
        wxFAIL_MSG( _T("invalid date range in GetHolidaysInRange") );

        return 0u;
    }

    holidays.Empty();

    // instead of checking all days, start with the first Sat after dtStart and
    // end with the last Sun before dtEnd
    wxDateTime dtSatFirst = dtStart.GetNextWeekDay(wxDateTime::Sat),
               dtSatLast = dtEnd.GetPrevWeekDay(wxDateTime::Sat),
               dtSunFirst = dtStart.GetNextWeekDay(wxDateTime::Sun),
               dtSunLast = dtEnd.GetPrevWeekDay(wxDateTime::Sun),
               dt;

    for ( dt = dtSatFirst; dt <= dtSatLast; dt += wxDateSpan::Week() )
    {
        holidays.Add(dt);
    }

    for ( dt = dtSunFirst; dt <= dtSunLast; dt += wxDateSpan::Week() )
    {
        holidays.Add(dt);
    }

    return holidays.GetCount();
}

#endif // wxUSE_DATETIME
