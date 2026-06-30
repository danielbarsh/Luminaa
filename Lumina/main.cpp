#include <SFML/Graphics.hpp>

void update(sf::RenderWindow& window);
void draw(sf::RenderWindow& window);


int main()
{
	//--------------------INITIALIZE-------------------------------------//
	sf::RenderWindow window(sf::VideoMode({ 1890, 1000 }), "RGP game");
	sf::CircleShape circle(50.0f);
	sf::RectangleShape rectangle({100,100});
	circle.setFillColor(sf::Color::Red);
	circle.setPosition({ 350,220 });
	rectangle.setFillColor(sf::Color::Red);
	rectangle.setOrigin({ 50,50 });
	rectangle.setPosition(sf::Vector2f(100, 100));

	//--------------------INITIALIZE-------------------------------------//
	while (window.isOpen()) {
		
		update(window);
	    

		window.clear(sf::Color::Black);
		window.draw(circle);
		window.draw(rectangle);
		window.display();
	}


	return 0;
}

void update(sf::RenderWindow& window) {

	while (const std::optional event = window.pollEvent()) {

		if (event->is<sf::Event::Closed>()) {
			window.close();
		}
 
	}
}

void draw(sf::RenderWindow& window) {

	window.clear(sf::Color::Red);

	

	window.display();

}

