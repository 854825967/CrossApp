#include "CATextView.h"
#include "basics/CAApplication.h"
#include "basics/CAScheduler.h"
#include "view/CAWindow.h"
#include "actions/CCActionInterval.h"
#include "CCEGLView.h"
#include <utility>
#include "shaders/CAShaderCache.h"
#include "platform/CAClipboard.h"


#if (CC_TARGET_PLATFORM != CC_PLATFORM_LINUX)
#include "platform/CAFTFontCache.h"
#endif

NS_CC_BEGIN

#pragma CATextView

CATextView::CATextView()
: m_pTextViewDelegate(NULL)
, m_pBackgroundView(NULL)
, m_pCursorMark(NULL)
, m_pImageView(NULL)
, m_cCursorColor(CAColor_black)
, m_cTextColor(ccc4(0, 0, 0, 255))
, m_cSpaceHolderColor(ccc4(193, 193, 193, 255))
, m_nInputType(KEY_BOARD_INPUT_NORMAL)
, m_keyboardType(KEY_BOARD_TYPE_NORMAL)
, m_szFontName("")
, m_iFontSize(24)
, m_iCurPos(0)
, m_bUpdateImage(false)
, m_iLineSpacing(0)
, m_bWordWrap(true)
, m_isTouchInSide(false)
, m_curSelCharRange(std::make_pair(0, 0))
, m_pCurTouch(NULL)
, m_pCurEvent(NULL)
, m_pTextSelView(NULL)
, m_pTextArrView(NULL)
{
	m_iLineHeight = CAImage::getFontHeight(m_szFontName.c_str(), m_iFontSize);
}


CATextView::~CATextView()
{
	m_pTextViewDelegate = NULL;
}

void CATextView::onEnterTransitionDidFinish()
{
	CAScrollView::onEnterTransitionDidFinish();
	CC_RETURN_IF(m_sPlaceHolder.empty());
	this->updateImage();

}

bool CATextView::resignFirstResponder()
{
	bool result = CAView::resignFirstResponder();
	if (result)
	{
		detachWithIME();
	}
	return result;
}

bool CATextView::becomeFirstResponder()
{
	bool result = CAView::becomeFirstResponder();
	if (result)
	{
		attachWithIME();
	}
	return result;
}

CATextView* CATextView::createWithFrame(const CCRect& rect)
{
	CATextView* pTextView = new CATextView();
	if (pTextView && pTextView->initWithFrame(rect))
	{
		pTextView->autorelease();
		return pTextView;
	}
	CC_SAFE_DELETE(pTextView);
	return NULL;
}

CATextView* CATextView::createWithCenter(const CCRect& rect)
{
	CATextView* pTextView = new CATextView();
	if (pTextView && pTextView->initWithCenter(rect))
	{
		pTextView->autorelease();
		return pTextView;
	}
	CC_SAFE_DELETE(pTextView);
	return NULL;
}

bool CATextView::init()
{
	if (!CAScrollView::init())
	{
		return false;
	}

	this->setShowsHorizontalScrollIndicator(false);
	this->setBounceHorizontal(false);
	this->setTouchMovedListenHorizontal(false);
	this->setBounceHorizontal(false);

    m_pImageView = new CAImageView();
    m_pImageView->setShaderProgram(CAShaderCache::sharedShaderCache()->programForKey(kCCShader_PositionTextureA8Color));
    m_pImageView->autorelease();
	this->addSubview(m_pImageView);

	m_pTextSelView = CATextSelViewEx::create();
	m_pTextSelView->setFrame(getFrame());
	this->addSubview(m_pTextSelView);

	m_pTextArrView = CATextArrowView::create();
	m_pTextArrView->setFrame(getFrame());
	this->addSubview(m_pTextArrView);

	return true;
}


bool CATextView::initWithFrame(const CCRect& frame)
{
	if (!CAScrollView::initWithFrame(frame))
	{
		return false;
	}
	initMarkSprite();
	return true;
}

bool CATextView::initWithCenter(const CCRect& rect)
{
	if (!CAScrollView::initWithCenter(rect))
	{
		return false;
	}
	initMarkSprite();
	return true;
}

void CATextView::initMarkSprite()
{
	if (m_pCursorMark == NULL)
	{
		m_pCursorMark = CAView::create();
		m_pCursorMark->setColor(m_cCursorColor);
		this->addSubview(m_pCursorMark);
        this->hideCursorMark();
	}
	m_pCursorMark->setFrame(CCRect(0, 0, 2, m_iLineHeight));
}

void CATextView::showCursorMark()
{
    if(!m_pCursorMark->isVisible()){
        m_pCursorMark->setVisible(true);
        m_pCursorMark->runAction(CCRepeat::create(CCBlink::create(0.8f, 1), 1048576));
    }
}

void CATextView::hideCursorMark()
{
    m_pCursorMark->setVisible(false);
    m_pCursorMark->stopAllActions();
}

void CATextView::updateImage()
{
	std::string text;
	if (m_szText.empty())
	{
		text = m_sPlaceHolder;
	}
	else
	{
		text = m_szText;
	}

	m_cFontColor = m_szText.empty() ? m_cSpaceHolderColor : m_cTextColor;
	if (m_pImageView)
	{
		m_pImageView->setColor(m_cFontColor);
	}

	float width = this->getBounds().size.width;

	CAImage* image = g_AFTFontCache.initWithString(text.c_str(),
		m_szFontName.c_str(),
		m_iFontSize,
		width,
		0,
		CATextAlignmentLeft,
		CAVerticalTextAlignmentTop,
		m_bWordWrap,
		m_iLineSpacing,
		false,
		false,
		false,
		&m_vLinesTextView);

	if (image == NULL || m_szText.empty())
	{
		m_vLinesTextView.clear();
	}
    m_pImageView->setColor(m_cFontColor);
	m_pImageView->setImage(image);
    CCRect rect = CCRectZero;
    rect.size = image->getContentSize();
    m_pImageView->setImageRect(rect);
    m_pImageView->setFrame(rect);
	setViewSize(rect.size);
	calcCursorPosition();
}


void CATextView::calcCursorPosition()
{
	int iCurLine = -1, iCurLineCharPos = 0;

	for (unsigned i = 0; i < m_vLinesTextView.size(); i++)
	{
		TextViewLineInfo& t = m_vLinesTextView[i];

		if (m_iCurPos >= t.iStartCharPos && m_iCurPos <= t.iEndCharPos)
		{
			iCurLine = i; iCurLineCharPos = t.iStartCharPos;
			break;
		}
	}

	float fHalfLineHeight = m_iLineHeight / 2.0f;
	float fLineSpaceValue = m_iLineHeight / 4.0f;

	CCPoint cCurPosition;
	if (iCurLine == -1)
	{
		cCurPosition.x = 0;
		cCurPosition.y = fHalfLineHeight;
	}
	else
	{
		std::string s = m_szText.substr(iCurLineCharPos, m_iCurPos - iCurLineCharPos);
		if (!s.empty() && s[0] == '\n')
		{
			s.erase(0, 1);
		}
		cCurPosition.x = getStringLength(s);
		cCurPosition.y = (m_iLineHeight + fLineSpaceValue)*iCurLine + fHalfLineHeight;
	}

	if (m_pCursorMark)
	{
		m_pCursorMark->setCenterOrigin(cCurPosition);
	}

	float w = getBounds().size.height;
	float y = cCurPosition.y - getContentOffset().y;
	if (y < 0 || y > w)
	{
		y = y < 0 ? fHalfLineHeight : w - fHalfLineHeight;
		setContentOffset(CCPointMake(0, cCurPosition.y-y), true);
	}
}


void CATextView::setFontSize(int var)
{
	m_iFontSize = var;
	m_iLineHeight = CAImage::getFontHeight(m_szFontName.c_str(), m_iFontSize);
	initMarkSprite();
	m_bUpdateImage = true;
}

int CATextView::getFontSize()
{
	return m_iFontSize;
}

void CATextView::setText(const std::string& var)
{
	CATextViewDelegate* pTemp = m_pTextViewDelegate;
	m_pTextViewDelegate = NULL;
	m_szText.clear();
	m_iCurPos = 0;
	m_vLinesTextView.clear();
	m_curSelCharRange = std::make_pair(0, 0);
	insertText(var.c_str(), (int)var.length());
	m_pTextViewDelegate = pTemp;
}

const std::string& CATextView::getText()
{
	return m_szText;
}

void CATextView::setPlaceHolder(const std::string& var)
{
	m_sPlaceHolder = var;
	this->updateImage();
}

const std::string& CATextView::getPlaceHolder()
{
	return m_sPlaceHolder;
}

void CATextView::setSpaceHolderColor(const CAColor4B &var)
{
	m_cSpaceHolderColor = var;
	this->updateImage();
}

const CAColor4B &CATextView::getSpaceHolderColor()
{
	return m_cSpaceHolderColor;
}

void CATextView::setLineSpacing(unsigned int var)
{
	m_iLineSpacing = var;
	m_bUpdateImage = true;
}

unsigned int CATextView::getLineSpacing()
{
	return m_iLineSpacing;
}

void CATextView::setWordWrap(bool var)
{
	m_bWordWrap = var;
	m_bUpdateImage = true;
}

bool CATextView::getWordWrap()
{
	return m_bWordWrap;
}

void CATextView::setFontName(const std::string& var)
{
	m_szFontName = var;
	m_iLineHeight = CAImage::getFontHeight(m_szFontName.c_str(), m_iFontSize);
	initMarkSprite();
	m_bUpdateImage = true;
}

const std::string& CATextView::getFontName()
{
	return m_szFontName;
}

void CATextView::setCursorColor(const CAColor4B &var)
{
	m_cCursorColor = var;
	if (m_pCursorMark)
	{
		m_pCursorMark->setColor(m_cCursorColor);
	}
}

const CAColor4B &CATextView::getCursorColor()
{
	return m_cCursorColor;
}

void CATextView::setTextColor(const CAColor4B &var)
{
	m_cTextColor = var;
	this->updateImage();
}

const CAColor4B &CATextView::getTextColor()
{
	return m_cTextColor;
}

void CATextView::setFontColor(const CAColor4B &var)
{
	m_cFontColor = var;
	if (m_pImageView)
	{
		m_pImageView->setColor(m_cFontColor);
	}
	m_bUpdateImage = true;
}

const CAColor4B &CATextView::getFontColor()
{
	return m_cFontColor;
}

bool CATextView::canAttachWithIME()
{
	return (m_pTextViewDelegate) ? m_pTextViewDelegate->onTextViewAttachWithIME(this) : true;
}

bool CATextView::canDetachWithIME()
{
	return (m_pTextViewDelegate) ? m_pTextViewDelegate->onTextViewDetachWithIME(this) : true;
}

void CATextView::didDetachWithIME()
{
    hideCursorMark();
}

void CATextView::didAttachWithIME()
{
    showCursorMark();
}

void CATextView::insertText(const char * text, int len)
{
    CCLog("insertText: %s, %d", text, len);
    if (len<=0)return;
	execCurSelCharRange();
	m_szText.insert(m_iCurPos, text, len);
 	m_iCurPos += len;
	m_curSelCharRange = std::make_pair(m_iCurPos, m_iCurPos);
	updateImage();
	m_pTextArrView->hideTextArrView();
}

void CATextView::willInsertText(const char* text, int len)
{
    CCLog("willInsertText: %s, %d", text, len);
	execCurSelCharRange();
	int iOldCurPos = m_iCurPos;
	insertText(text, len);
	m_curSelCharRange = std::make_pair(iOldCurPos, m_iCurPos);
	m_pTextSelView->showTextViewMark(getZZCRect());
}

void CATextView::AndroidWillInsertText(int start, const char* str, int before, int count)
{

	CCAssert(str != NULL, "");
	CCAssert(count > 0, "");

 	insertText(str, (int)strlen(str));

}

void CATextView::deleteBackward()
{
	if (m_iCurPos == 0 || m_szText.empty())
		return;

	CC_RETURN_IF(m_pTextViewDelegate && m_pTextViewDelegate->onTextViewDeleteBackward(this, m_szText.c_str(), (int)m_szText.length()));

	if (execCurSelCharRange())
		return;

	int nDeleteLen = 1;
	while (0x80 == (0xC0 & m_szText.at(m_iCurPos - nDeleteLen)))
	{
		++nDeleteLen;
	}

	m_szText.erase(m_iCurPos - nDeleteLen, nDeleteLen);
	m_iCurPos -= nDeleteLen;
	updateImage();
	m_pTextArrView->hideTextArrView();
}

void CATextView::getKeyBoardHeight(int height)
{
	CAView::becomeFirstResponder();
    
    if( m_pTextViewDelegate)
    {
        m_pTextViewDelegate->getKeyBoardHeight(height);
    }
}

void CATextView::getKeyBoradReturnCallBack()
{
	if (m_pTextViewDelegate && m_pTextViewDelegate->keyBoardCallBack(this))
	{
		return;
	}
	else
	{
		this->resignFirstResponder();
	}
}

const char* CATextView::getContentText()
{
	return m_szText.c_str();
}

float CATextView::maxSpeed(float dt)
{
    return (CCPoint(m_obContentSize).getLength() * 8 * dt);
}

float CATextView::maxSpeedCache(float dt)
{
    return (maxSpeed(dt) * 3.0f);
}

float CATextView::decelerationRatio(float dt)
{
    return 2.0f * dt;
}


int CATextView::getCurrentByPointY(int y)
{
	int iCurLine = y / (m_iLineHeight*1.25f);
	if (m_vLinesTextView.empty())
	{
		iCurLine = 0;
	}
	else if (iCurLine >= m_vLinesTextView.size())
	{
		iCurLine = (int)m_vLinesTextView.size() - 1;
	}
	return iCurLine;
}

void CATextView::calculateSelChars(const CCPoint& point, int& l, int& r, int& p)
{
	float y = point.y + getContentOffset().y;
	int iCurLine = getCurrentByPointY(y);
	int iHalfCharSize = 0;
	int iStartPos = 0;
	if (!m_vLinesTextView.empty())
	{
		p = iStartPos = m_vLinesTextView[iCurLine].iStartCharPos;
		std::vector<TextAttribute>& v = m_vLinesTextView[iCurLine].TextAttrVect;
		for (int i = 0, iStringLeftX = 0; i < v.size(); i++)
		{
			TextAttribute& t = v[i];
			if (point.x >= iStringLeftX - iHalfCharSize && point.x < iStringLeftX + t.charlength / 2)
			{
				break;
			}

			iStringLeftX += t.charlength;
			p += t.charSize;
			iHalfCharSize = t.charlength / 2;
		}
	}

	std::string s = m_szText.substr(iStartPos, p - iStartPos);
	if (!s.empty() && s[0] == '\n')
	{
		s.erase(0, 1);
	}
	l = iCurLine;
	r = getStringLength(s);
}


bool CATextView::execCurSelCharRange()
{
	CATextToolBarView::hideTextToolBar();

	if (m_curSelCharRange.first == m_curSelCharRange.second)
		return false;

	int iOldCurPos = m_curSelCharRange.first;
	std::string cszText = m_szText.erase(m_curSelCharRange.first, m_curSelCharRange.second - m_curSelCharRange.first);
	setText(cszText);

	m_iCurPos = iOldCurPos;
    this->showCursorMark();
	calcCursorPosition();
    m_pTextSelView->hideTextSelView();
	return true;
}

void CATextView::ccStartSelect()
{
	if (m_szText.empty())
		return;

	int iLastPos = m_iCurPos;
	if (m_iCurPos == 0)
	{
		int nMoveLen = 1;
		for (std::string::size_type n = m_iCurPos + nMoveLen; n < m_szText.length() && 0x80 == (0xC0 & m_szText.at(n));)
		{
			++nMoveLen;
			n = m_iCurPos + nMoveLen;
		}
		m_iCurPos += nMoveLen;
		m_curSelCharRange.first = iLastPos;
		m_curSelCharRange.second = m_iCurPos;
	}
	else
	{
		int nMoveLen = 1;
		while (0x80 == (0xC0 & m_szText.at(m_iCurPos - nMoveLen)))
		{
			++nMoveLen;
		}
		m_iCurPos -= nMoveLen;
		m_curSelCharRange.first = m_iCurPos;
		m_curSelCharRange.second = iLastPos;
	}

	m_pTextArrView->hideTextArrView();
	m_pTextSelView->showTextSelView(getZZCRect(), m_iLineHeight);
	hideCursorMark();
}

void CATextView::ccTouchTimer(float interval)
{
	CAScheduler::unschedule(schedule_selector(CATextView::ccTouchTimer), this);
	ccTouchPress(m_pCurTouch, m_pCurEvent);
}

std::pair<int, int> CATextView::getLineAndPos(int iPos)
{
	std::pair<int, int> t;

	t.first = t.second = 0;
	for (int i = 0; i < m_vLinesTextView.size(); i++)
	{
		if (iPos >= m_vLinesTextView[i].iStartCharPos && iPos <= m_vLinesTextView[i].iEndCharPos)
		{
			t.first = i; t.second = m_vLinesTextView[i].iStartCharPos;
			break;
		}
	}
	return t;
}

std::vector<CCRect> CATextView::getZZCRect()
{
	CCSize size = getFrame().size;
	std::pair<int, int> t1 = getLineAndPos(m_curSelCharRange.first);
	std::pair<int, int> t2 = getLineAndPos(m_curSelCharRange.second);

	std::string s1 = m_szText.substr(t1.second, m_curSelCharRange.first - t1.second);
	if (!s1.empty() && s1[0] == '\n') s1.erase(0, 1);
	int l1 = getStringLength(s1);

	std::string s2 = m_szText.substr(t2.second, m_curSelCharRange.second - t2.second);
	if (!s2.empty() && s2[0] == '\n') s2.erase(0, 1);
	int l2 = getStringLength(s2);

	std::vector<CCRect> vr;
	if (t1.first == t2.first)
	{
		vr.push_back(CCRect(l1, m_iLineHeight*1.25f*t1.first, l2 - l1, m_iLineHeight));
	}
	else
	{
		vr.push_back(CCRect(l1, m_iLineHeight*1.25f*t1.first, size.width - l1, m_iLineHeight*1.25f));

		int i = t1.first + 1;
		for (; i < t2.first; i++)
		{
			vr.push_back(CCRect(0, m_iLineHeight*1.25f*i, size.width, m_iLineHeight*1.25f));
		}
		vr.push_back(CCRect(0, m_iLineHeight*1.25f*i, l2, m_iLineHeight));
	}
	for (int i = 0; i < vr.size(); i++)
	{
		vr[i].origin.y += getContentOffset().y;
	}
	return vr;
}

bool CATextView::ccTouchBegan(CATouch *pTouch, CAEvent *pEvent)
{
    this->setTouchMovedListenVertical(false);
    this->setBounceVertical(false);

    CAScheduler::schedule(schedule_selector(CATextView::ccTouchTimer), this, 0, 0, 1.5f);
    m_pCurTouch = pTouch;
    m_pCurEvent = pEvent;

    if (m_pTextSelView->touchSelectText(pTouch))
    {
        CATextToolBarView *pToolBar = CATextToolBarView::create();
        pToolBar->addButton(UTF8("\u526a\u5207"), this, callfunc_selector(CATextView::ccCutToClipboard));
        pToolBar->addButton(UTF8("\u590d\u5236"), this, callfunc_selector(CATextView::ccCopyToClipboard));
        pToolBar->addButton(UTF8("\u7c98\u8d34"), this, callfunc_selector(CATextView::ccPasteFromClipboard));
        pToolBar->show(this);
		return false;
    }
    else if (m_pTextSelView->isTextViewShow())
    {
        return true;
    }
    return CAScrollView::ccTouchBegan(pTouch, pEvent);
}


void CATextView::ccTouchMoved(CATouch *pTouch, CAEvent *pEvent)
{
	CAScheduler::unschedule(schedule_selector(CATextView::ccTouchTimer), this);
    CAScrollView::ccTouchMoved(pTouch, pEvent);
}

void CATextView::ccTouchEnded(CATouch *pTouch, CAEvent *pEvent)
{
	CAScheduler::unschedule(schedule_selector(CATextView::ccTouchTimer), this);

    if (CATextToolBarView::isTextToolBarShow())
		return;
	
	CCPoint point = this->convertTouchToNodeSpace(pTouch);

	if (!isScrollWindowNotOutSide())
	{
		if (this->getBounds().containsPoint(point))
		{
			becomeFirstResponder();
			if (isFirstResponder() && !m_pTextSelView->isTextViewShow())
			{
                this->showCursorMark();

				int iCurLine = 0; int iCurPosX = 0;
				calculateSelChars(point, iCurLine, iCurPosX, m_iCurPos);
				m_pCursorMark->setCenterOrigin(CCPoint(iCurPosX, m_iLineHeight*1.25f*iCurLine + m_iLineHeight / 2));

                CCPoint pt = m_pCursorMark->getCenterOrigin();
                m_pTextArrView->showTextArrView(CCPoint(pt.x, pt.y + m_iLineHeight*1.2f + getContentOffset().y));
                m_curSelCharRange = std::pair<int,int>(m_iCurPos, m_iCurPos);
			}
		}
		else
		{
			m_pTextArrView->hideTextArrView();
			m_pTextSelView->hideTextSelView();

			if (resignFirstResponder())
			{
                this->hideCursorMark();
			}
		}
        
	}

	CAScrollView::ccTouchEnded(pTouch, pEvent);
}

void CATextView::ccTouchCancelled(CATouch *pTouch, CAEvent *pEvent)
{
	CAScheduler::unschedule(schedule_selector(CATextView::ccTouchTimer), this);
	CAScrollView::ccTouchCancelled(pTouch, pEvent);
}

void CATextView::ccTouchPress(CATouch *pTouch, CAEvent *pEvent)
{
	if (m_pTextSelView->isTextViewShow())
		return;

    becomeFirstResponder();
	CATextToolBarView *pToolBar = CATextToolBarView::create();
	if (m_szText.empty())
	{
		pToolBar->addButton(UTF8("\u7c98\u8d34"), this, callfunc_selector(CATextView::ccPasteFromClipboard));
	}
	else
	{
		pToolBar->addButton(UTF8("\u7c98\u8d34"), this, callfunc_selector(CATextView::ccPasteFromClipboard));
		pToolBar->addButton(UTF8("\u5168\u9009"), this, callfunc_selector(CATextView::ccSelectAll));
		pToolBar->addButton(UTF8("\u9009\u62e9"), this, callfunc_selector(CATextView::ccStartSelect));
	}
	pToolBar->show(this);
}

bool CATextView::attachWithIME()
{
	bool bRet = CAIMEDelegate::attachWithIME();
	if (bRet)
	{
		// open keyboard
		CCEGLView * pGlView = CAApplication::getApplication()->getOpenGLView();
		if (pGlView)
		{
#if( CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID || CC_TARGET_PLATFORM == CC_PLATFORM_IOS )

            pGlView->setIMEKeyboardReturnEnter();

            if (getKeyboardType() ==KEY_BOARD_TYPE_NORMAL)
            {
                pGlView->setIMEKeyboardDefault();
            }
            else if (getKeyboardType() ==KEY_BOARD_TYPE_NUMBER)
            {
                pGlView->setIMEKeyboardNumber();
            }
            else if(getKeyboardType() ==KEY_BOARD_TYPE_ALPHABET)
            {
                pGlView->setIMEKeyboardAlphabet();
            }
            
            if (getKeyboardReturnType() ==KEY_BOARD_RETURN_SEND)
            {
                pGlView->setIMEKeyboardReturnSend();
            }
            else if (getKeyboardReturnType() ==KEY_BOARD_RETURN_SEARCH)
            {
                pGlView->setIMEKeyboardReturnSearch();
            }
            else if(getKeyboardReturnType() ==KEY_BOARD_RETURN_DONE)
            {
                pGlView->setIMEKeyboardReturnDone();
            }

#endif
			pGlView->setIMEKeyboardState(true);
		}
	}
	return bRet;
}

bool CATextView::detachWithIME()
{
	bool bRet = CAIMEDelegate::detachWithIME();
	if (bRet)
	{
		// close keyboard
		CCEGLView * pGlView = CAApplication::getApplication()->getOpenGLView();
		if (pGlView)
		{
			pGlView->setIMEKeyboardState(false);
		}
		m_pTextArrView->hideTextArrView();
	}
	return bRet;
}


void CATextView::selectAll()
{
	m_curSelCharRange.first = 0;
	m_curSelCharRange.second = m_iCurPos = (int)m_szText.length();

	m_pTextArrView->hideTextArrView();
	m_pTextSelView->showTextSelView(getZZCRect(), m_iLineHeight);
	this->hideCursorMark();
}

void CATextView::cursorMoveBackward()
{
	if (m_iCurPos == 0 || m_szText.empty())
		return;

	int iPrevPos = (int)m_iCurPos, nMoveLen = 1;
	while (0x80 == (0xC0 & m_szText.at(m_iCurPos - nMoveLen)))
	{
		++nMoveLen;
	}
	m_iCurPos -= nMoveLen;

	m_curSelCharRange.first = m_curSelCharRange.second = 0;
	calcCursorPosition();
}

void CATextView::cursorMoveForward()
{
	if (m_iCurPos == m_szText.length() || m_szText.empty())
		return;

	int iPrevPos = (int)m_iCurPos, nMoveLen = 1;
	for (std::string::size_type n = m_iCurPos + nMoveLen; n < m_szText.length() && 0x80 == (0xC0 & m_szText.at(n));)
	{
		++nMoveLen;
		n = m_iCurPos + nMoveLen;
	}
	m_iCurPos += nMoveLen;

	m_curSelCharRange.first = m_curSelCharRange.second = 0;
	calcCursorPosition();
}

void CATextView::moveSelectChars(bool isLeftBtn, const CCPoint& pt)
{
	int l, r, p;
	calculateSelChars(convertToNodeSpace(pt), l, r, p);

	if (isLeftBtn)
	{
		if (p < m_curSelCharRange.second)
			m_curSelCharRange.first = p;
	}
	else
	{
		if (p > m_curSelCharRange.first)
			m_curSelCharRange.second = p;
	}

	m_iCurPos = p;
	calcCursorPosition();

	m_pTextArrView->hideTextArrView();
	m_pTextSelView->showTextSelView(getZZCRect(), m_iLineHeight);
	hideCursorMark();
}

void CATextView::moveArrowBtn(const CCPoint& pt)
{
	int iCurLine = 0; int iCurPosX = 0;
	calculateSelChars(convertToNodeSpace(pt), iCurLine, iCurPosX, m_iCurPos);
	m_curSelCharRange = std::make_pair(0, 0);
	calcCursorPosition();

	m_pCursorMark->stopAllActions();
	m_pCursorMark->setCenterOrigin(CCPoint(iCurPosX, m_iLineHeight*1.25f*iCurLine + m_iLineHeight / 2));
	m_pCursorMark->setVisible(true);

	CCPoint ptArr = m_pCursorMark->getCenterOrigin();
	m_pTextArrView->showTextArrView(CCPoint(ptArr.x, ptArr.y + m_iLineHeight*1.2f + getContentOffset().y));
}

void CATextView::copyToClipboard()
{
	std::string cszText = m_szText.substr(m_curSelCharRange.first, m_curSelCharRange.second - m_curSelCharRange.first);
	CAClipboard::setText(cszText);
}

void CATextView::cutToClipboard()
{
	copyToClipboard();

	execCurSelCharRange();
}

void CATextView::pasteFromClipboard()
{
	std::string cszText = CAClipboard::getText();
	insertText(cszText.c_str(), (int)cszText.size());
}

int CATextView::getStringLength(const std::string &var)
{
#if (CC_TARGET_PLATFORM != CC_PLATFORM_LINUX)
	return g_AFTFontCache.getStringWidth(m_szFontName.c_str(), m_iFontSize, var);
#else
	CAImage *image = CAImage::createWithString(var.c_str(),
		"",
		m_iFontSize,
		CCSizeZero,
		CATextAlignmentLeft,
		CAVerticalTextAlignmentCenter);
	if (image != NULL)
	{
		return image->getContentSize().width;
	}
	return 0;
#endif
}


void CATextView::visit()
{
	if (m_bUpdateImage)
	{
		m_bUpdateImage = false;
		this->updateImage();
	}
	CAScrollView::visit();
}

void CATextView::keyboardDidShow(CCIMEKeyboardNotificationInfo& info)
{
    if (!m_isTouchInSide)
    {
        m_isTouchInSide = true;
    }
}

void CATextView::keyboardWillHide(CCIMEKeyboardNotificationInfo& info)
{
    m_curSelCharRange = std::make_pair(m_iCurPos, m_iCurPos);
    execCurSelCharRange();
}

void CATextView::keyboardDidHide(CCIMEKeyboardNotificationInfo& info)
{
    if(m_isTouchInSide)
    {
        m_isTouchInSide = false;
        CAView::resignFirstResponder();

    }
}

NS_CC_END
